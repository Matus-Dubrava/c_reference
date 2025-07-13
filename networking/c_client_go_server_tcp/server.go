package main

import (
	"fmt"
	"io"
	"net"
)

func HandleConnection(conn net.Conn) {
	for {
		buf := make([]byte, 1024)
		_, err := conn.Read(buf)
		if err != nil {
			if err == io.EOF {
				fmt.Println("client disconnected")
				break
			}
			fmt.Println("read error: ", err)
			break
		}
		fmt.Println(string(buf))

		resp := []byte("ack")
		_, err = conn.Write(resp)
		if err != nil {
			fmt.Println("failed to send ack to client")
		}
	}
}

func main() {
	ln, err := net.Listen("tcp", "127.0.0.1:10000")
	if err != nil {
		panic("unable to start listening on port")
	}

	for {
		fmt.Println("waiting for connections")
		conn, err := ln.Accept()
		if err != nil {
			panic("failed to accept connection")
		}
		defer conn.Close()

		go HandleConnection(conn)
	}
}
