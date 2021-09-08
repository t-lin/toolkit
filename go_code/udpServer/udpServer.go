package main

import (
	"fmt"
	"net"
)

func main() {
	//listenEndpoint := "0.0.0.0:1234"
	listenEndpoint := "127.0.0.1:" // randomly choose port
	udpAddr, err := net.ResolveUDPAddr("udp", listenEndpoint)
	if err != nil {
		fmt.Printf("ERROR: Unable to resolve UDP address\n%v\n", err)
		return
	}

	conn, err := net.ListenUDP("udp", udpAddr)
	if err != nil {
		fmt.Printf("ERROR: Unable to listen on %s\n%v\n", udpAddr, err)
		return
	}
	defer conn.Close()

	fmt.Println("Listening for UDP datagrams on:", conn.LocalAddr())

	buf := make([]byte, 2048)
	for {
		size, from, err := conn.ReadFrom(buf)
		if err != nil {
			return
		}
		data := buf[:size]

		fmt.Printf("Received from %s: %s\n", from, string(data))

		response := []byte(fmt.Sprintf("Received data at SERVER-1: %s\n", string(data)))
		_, err = conn.WriteTo(response, from)
		if err != nil {
			fmt.Printf("ERROR: Could not send response to %v\n", err)
			return
		}
	}
}
