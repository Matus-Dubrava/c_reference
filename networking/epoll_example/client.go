package main

import (
	"fmt"
	"math/rand"
	"net"
	"time"
)

func create_client() {
	conn, err := net.Dial("tcp", "127.0.0.1:10000")
	if err != nil {
		fmt.Println("failed to establish connection")
		return
	}
	defer conn.Close()

	client_id := rand.Intn(100)

	for {
		msg := []byte(fmt.Sprintf("hello from client %d", client_id))
		n, err := conn.Write(msg)
		if err != nil {
			fmt.Println("failed to send message")
			return
		} else {
			fmt.Println(fmt.Sprintf("sent %d bytes", n))
		}

		res := make([]byte, 1024)
		conn.Read(res)
		fmt.Println(client_id, "received response:", string(res))

		time.Sleep(1 * time.Second)
	}
}

func main() {
	rand.Seed(time.Now().UnixNano())

	n_clients := 24
	for n := 0; n < n_clients; n++ {
		fmt.Println("creating client ", n)
		go create_client()
	}

	select {}
}
