package main

import (
	"fmt"
	"net"
	"time"
)

func main() {
	conn, err := net.Dial("tcp", "127.0.0.1:10000")
	if err != nil {
		fmt.Println("failed to establish connection")
		return
	}
	defer conn.Close()

	for {
		msg := []byte("hello there")
		n, err := conn.Write(msg)
		if err != nil {
			fmt.Println("failed to send message to server")
			return
		} else {
			fmt.Println(fmt.Sprintf("send %d bytes", n))
			time.Sleep(1 * time.Second)
		}
	}
}
