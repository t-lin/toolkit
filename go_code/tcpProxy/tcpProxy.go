package main

import (
	"fmt"
	"log"
	"net"
	"os"
	"syscall"
)

// Listens to the listening socket and redirects to remote addr
type Forwarder struct {
	// Use TCPAddr instead for endpoint addresses?
	ListenAddr string
	tcpWorker  func(net.Listener, string)
}

// Maps a remote addr to existing ServiceProxy for that addr
var serv2Fwd = make(map[string]Forwarder)

// HTTP listening endpoint for control messages
// TODO: Make this configurable? i.e. proxy available to non-localhost?
var ctrlHost = "127.0.0.1" // Port will be passed via CLI

// TCP data forwarder
// Simplified version of pipe() from https://github.com/jpillora/go-tcp-proxy/blob/master/proxy.go
func tcpFwdData(src, dst net.Conn) {
	var err error
	var nBytes, nBytesW, n int
	buf := make([]byte, 0xffff) // 64k buffer
	for {
		nBytesW = 0

		// NOTE: Using io.Copy or io.CopyBuffer slows down *a lot* after 2-3 runs.
		//       Not clear why right now. Thus, do the copy manually.
		nBytes, err = src.Read(buf)
		if err != nil {
			log.Printf("Connection %s <=> %s closed", src.LocalAddr(), src.RemoteAddr())
			return
		}
		data := buf[:nBytes]

		for nBytesW < nBytes {
			n, err = dst.Write(data)
			if err != nil {
				log.Printf("Connection %s <=> %s closed", dst.LocalAddr(), dst.RemoteAddr())
				return
			}

			nBytesW += n
		}
	}
}

// Connection forwarder
func tcpFwdConnToServ(lConn net.Conn, targetAddr string) {
	defer lConn.Close()
	log.Printf("Accepted TCP conn: %s <=> %s\n", lConn.LocalAddr(), lConn.RemoteAddr())

	// Resolve and open connection to destination service
	rAddr, err := net.ResolveTCPAddr("tcp", targetAddr)
	if err != nil {
		log.Printf("ERROR: Unable to resolve target address %s\n%v\n", targetAddr, err)
		return
	}

	rConn, err := net.DialTCP("tcp", nil, rAddr)
	if err != nil {
		log.Printf("ERROR: Unable to dial target address %s\n%v\n", targetAddr, err)
		return
	}
	defer rConn.Close()

	// Forward data in each direction
	go tcpFwdData(lConn, rConn)
	tcpFwdData(rConn, lConn)
}

// Implementation of ServiceProxy
func tcpServiceProxy(listen net.Listener, targetAddr string) {
	defer listen.Close()

	for {
		conn, err := listen.Accept()
		if err != nil {
			if err == syscall.EINVAL {
				log.Printf("Listener (%s) closed", listen.Addr())
				break
			}
			log.Printf("Listener (%s) unable to accept connection\n%v\n", listen.Addr(), err)
			continue
		}

		// Forward connection
		go tcpFwdConnToServ(conn, targetAddr)
	}
}

// Open TCP tunnel to service and open local TCP listening port
// NOTE: Currently doesn't support SSL/TLS
//
// TODO: In the future, perhaps the connection type info (TCP/UDP) can be stored in hash-lookup?
// TODO: In the future, move towards proxy-to-proxy implementation
//       This may be useful? https://github.com/libp2p/go-libp2p-gostream
// Start separate goroutine for handling connections and proxying to service
//
// Returns a string of the new TCP listening endpoint address
func openTCPProxy(serviceAddr string) (string, error) {
	var listenAddr string
	serviceKey := "tcp://" + serviceAddr
	if _, exists := serv2Fwd[serviceKey]; !exists {
		listen, err := net.Listen("tcp", ctrlHost+":") // automatically choose port
		if err != nil {
			return "", fmt.Errorf("Unable to open TCP listening port\n")
		}

		listenAddr = listen.Addr().String()
		serv2Fwd[serviceKey] = Forwarder{
			ListenAddr: listenAddr,
			tcpWorker:  tcpServiceProxy,
		}
		go serv2Fwd[serviceKey].tcpWorker(listen, serviceAddr)
	} else {
		listenAddr = serv2Fwd[serviceKey].ListenAddr
	}

	return listenAddr, nil
}

func main() {
	if len(os.Args) != 2 {
		fmt.Printf("Requires one argument, the destination endpoint (e.g. 127.0.0.1:8080)\n")
		return
	}

	listenAddr, err := openTCPProxy(os.Args[1])
	if err != nil {
		panic(err)
	}
	fmt.Printf("New listening endpoint at: %s\n", listenAddr)

	select {}

	return
}
