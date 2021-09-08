package main

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"os/signal"
	"sort"
	"strings"
	"sync"
	"syscall"
	"time"

	core "github.com/ipfs/go-ipfs/core"
	fsrepo "github.com/ipfs/go-ipfs/repo/fsrepo"
	logging "github.com/ipfs/go-log"

	//"code.google.com/p/go.net/context"
	"golang.org/x/net/context"

	swarm "gx/ipfs/QmSwZMWwFZSUpe5muU2xgTUwppH24KfMwdPXiwbEp2c6G5/go-libp2p-swarm"
	ma "gx/ipfs/QmWWQ2Txc2c6tqjsBpzg5Ar652cHPGNsQQp2SejkNmkUMb/go-multiaddr"
)

var log = logging.Logger("tlin-test-log")

// IntrHandler helps set up an interrupt handler that can
// be cleanly shut down through the io.Closer interface.
type IntrHandler struct {
	sig chan os.Signal
	wg  sync.WaitGroup
}

func NewIntrHandler() *IntrHandler {
	ih := &IntrHandler{}
	ih.sig = make(chan os.Signal, 1)
	return ih
}

func (ih *IntrHandler) Close() error {
	close(ih.sig)
	ih.wg.Wait()
	return nil
}

// Handle starts handling the given signals, and will call the handler
// callback function each time a signal is catched. The function is passed
// the number of times the handler has been triggered in total, as
// well as the handler itself, so that the handling logic can use the
// handler's wait group to ensure clean shutdown when Close() is called.
func (ih *IntrHandler) Handle(handler func(count int, ih *IntrHandler), sigs ...os.Signal) {
	signal.Notify(ih.sig, sigs...)
	ih.wg.Add(1)
	go func() {
		defer ih.wg.Done()
		count := 0
		for range ih.sig {
			count++
			handler(count, ih)
		}
		signal.Stop(ih.sig)
	}()
}

func setupInterruptHandler(ctx context.Context) (io.Closer, context.Context) {
	intrh := NewIntrHandler()
	ctx, cancelFunc := context.WithCancel(ctx)

	handlerFunc := func(count int, ih *IntrHandler) {
		switch count {
		case 1:
			fmt.Println() // Prevent un-terminated ^C character in terminal

			ih.wg.Add(1)
			go func() {
				defer ih.wg.Done()
				cancelFunc()
			}()

		default:
			fmt.Println("Received another interrupt before graceful shutdown, terminating...")
			os.Exit(-1)
		}
	}

	intrh.Handle(handlerFunc, syscall.SIGHUP, syscall.SIGINT, syscall.SIGTERM)

	return intrh, ctx
}

func main() {
	//ctx, cancel := context.WithCancel(context.Background())
	//defer cancel()
	ctx := context.Background()

	// Setup interrupt handler
	intrh, ctx := setupInterruptHandler(ctx)
	defer intrh.Close()

	// Basic ipfsnode setup
	repo, err := fsrepo.Open("~/.ipfs")
	if err != nil {
		panic(err)
	}

	cfg := &core.BuildCfg{
		Repo:   repo,
		Online: true,
	}

	node, err := core.NewNode(ctx, cfg)

	if err != nil {
		panic(err)
	}

	defer func() {
		fmt.Println("Closing node...")
		node.Close()
	}()

	go func() {
		<-ctx.Done()
		fmt.Println("Received interrupt signal")
	}()

	for true {
		select {
		case <-ctx.Done():
			fmt.Println("Also detected interrupt...")
			return
		default:
			fmt.Println("=============")
			//printSwarmAddrs(node)
			printSwarmPeers(node)
			time.Sleep(1000 * time.Millisecond)
		}
	}

	/*
		list, err := corenet.Listen(nd, "/app/whyrusleeping")
		if err != nil {
			panic(err)
		}

		fmt.Printf("I am peer: %s\n", nd.Identity.Pretty())

		for {
			con, err := list.Accept()
			if err != nil {
				fmt.Println(err)
				return
			}

			defer con.Close()

			fmt.Fprintln(con, "Hello! This is whyrusleepings awesome ipfs service")
			fmt.Printf("Connection from: %s\n", con.Conn().RemotePeer())
		}
	*/

}

// printSwarmAddrs prints the addresses of the host
func printSwarmAddrs(node *core.IpfsNode) {
	if !node.OnlineMode() {
		fmt.Println("Swarm not listening, running in offline mode.")
		return
	}

	var lisAddrs []string
	ifaceAddrs, err := node.PeerHost.Network().InterfaceListenAddresses()
	if err != nil {
		log.Errorf("failed to read listening addresses: %s", err)
	}
	for _, addr := range ifaceAddrs {
		lisAddrs = append(lisAddrs, addr.String())
	}
	sort.Sort(sort.StringSlice(lisAddrs))
	for _, addr := range lisAddrs {
		fmt.Printf("Swarm listening on %s\n", addr)
	}

	var addrs []string
	for _, addr := range node.PeerHost.Addrs() {
		addrs = append(addrs, addr.String())
	}
	sort.Sort(sort.StringSlice(addrs))
	for _, addr := range addrs {
		fmt.Printf("Swarm announcing %s\n", addr)
	}

}

type streamInfo struct {
	Protocol string
}

type connInfo struct {
	Addr    string
	Peer    string
	Latency string
	Muxer   string
	Streams []streamInfo
}

func (ci *connInfo) Less(i, j int) bool {
	return ci.Streams[i].Protocol < ci.Streams[j].Protocol
}

func (ci *connInfo) Len() int {
	return len(ci.Streams)
}

func (ci *connInfo) Swap(i, j int) {
	ci.Streams[i], ci.Streams[j] = ci.Streams[j], ci.Streams[i]
}

type connInfos struct {
	Peers []connInfo
}

func (ci connInfos) Less(i, j int) bool {
	return ci.Peers[i].Addr < ci.Peers[j].Addr
}

func (ci connInfos) Len() int {
	return len(ci.Peers)
}

func (ci connInfos) Swap(i, j int) {
	ci.Peers[i], ci.Peers[j] = ci.Peers[j], ci.Peers[i]
}

func printSwarmPeers(node *core.IpfsNode) {
	conns := node.PeerHost.Network().Conns()

	var out connInfos
	for _, c := range conns {
		pid := c.RemotePeer()
		addr := c.RemoteMultiaddr()

		ci := connInfo{
			Addr: addr.String(),
			Peer: pid.Pretty(),
		}

		swcon, ok := c.(*swarm.Conn)
		if ok {
			ci.Muxer = fmt.Sprintf("%T", swcon.StreamConn().Conn())
		}

		//if verbose || latency {
		if true {
			lat := node.Peerstore.LatencyEWMA(pid)
			if lat == 0 {
				ci.Latency = "n/a"
			} else {
				ci.Latency = lat.String()
			}
		}

		/*if verbose || streams {
			strs, err := c.GetStreams()
			if err != nil {
				res.SetError(err, cmdkit.ErrNormal)
				return
			}

			for _, s := range strs {
				ci.Streams = append(ci.Streams, streamInfo{Protocol: string(s.Protocol())})
			}
		}*/
		sort.Sort(&ci)
		out.Peers = append(out.Peers, ci)
	}

	sort.Sort(&out)

	buf := new(bytes.Buffer)
	pipfs := ma.ProtocolWithCode(ma.P_IPFS).Name
	for _, info := range out.Peers {
		ids := fmt.Sprintf("/%s/%s", pipfs, info.Peer)
		if strings.HasSuffix(info.Addr, ids) {
			fmt.Fprintf(buf, "%s", info.Addr)
		} else {
			fmt.Fprintf(buf, "%s%s", info.Addr, ids)
		}
		if info.Latency != "" {
			fmt.Fprintf(buf, " %s", info.Latency)
		}
		fmt.Fprintln(buf)

		for _, s := range info.Streams {
			if s.Protocol == "" {
				s.Protocol = "<no protocol name>"
			}

			fmt.Fprintf(buf, "  %s\n", s.Protocol)
		}
	}

	fmt.Println(buf)
	fmt.Printf("Number of peers: %d\n\n", len(out.Peers))
}
