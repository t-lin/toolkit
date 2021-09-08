package main

import (
	"fmt"
	"net"
)

func connHandler(conn net.Conn) {
	defer conn.Close()
	fmt.Println("New connection from", conn.RemoteAddr())

    buf := make([]byte, 100)
	for {
		size, err := conn.Read(buf)
		if err != nil {
			return
		}
        fmt.Println("Received:", string(buf[:size]))

		_, err = conn.Write([]byte("Received data at SERVER-1\n"))
        if err != nil {
            return
        }
	}
}

func main() {
    listenEndpoint := "0.0.0.0:1234"

	listen, err := net.Listen("tcp", listenEndpoint)
	if err != nil {
        fmt.Printf("ERROR: Unable to listen on %s\n%v\n", listenEndpoint, err)
        return
	}
	defer listen.Close()

    fmt.Println("Listening on:", listenEndpoint)

	for {
		conn, err := listen.Accept()
		if err != nil {
            fmt.Printf("ERROR: Unable to accept connection\n%v\n", err)
		}

		go connHandler(conn)
	}
}


