package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"net"
	"os"
	"strconv"
)

const (
	serverAddressLength      = 20
	aesKeyLength             = 16
	zero                byte = 0
	serverPort               = 55555
)

func udpSend(conn *net.UDPConn, address net.Addr, data []byte) {
	fmt.Printf("Sending configuration to %v\n", address.String())
	_, err := conn.WriteTo(data, address)
	if err != nil {
		fmt.Println(err)
	}
}

func main() {
	l := len(os.Args)
	if l < 4 || l > 5 {
		fmt.Println("Usage: sensor_config_updater server_ip port aes_key_file_name [out_file_name]")
		os.Exit(1)
	}

	serverIp := os.Args[1]
	if len(serverIp) > 16 {
		panic("Server IP is too long")
	}
	port, err := strconv.Atoi(os.Args[2])
	if err != nil {
		panic(err)
	}
	if port <= 0 || port > 65535 {
		panic("Wrong port value")
	}
	aesKey, err := os.ReadFile(os.Args[3])
	if err != nil {
		panic(err)
	}
	if len(aesKey) != aesKeyLength {
		panic("Wrong AES key length")
	}
	var config bytes.Buffer
	_, err = config.WriteString(serverIp)
	if err != nil {
		panic(err)
	}
	for config.Len() < serverAddressLength {
		err = config.WriteByte(zero)
		if err != nil {
			panic(err)
		}
	}
	err = binary.Write(&config, binary.LittleEndian, uint16(port))
	if err != nil {
		panic(err)
	}
	config.Write(aesKey)
	if l == 5 {
		err = os.WriteFile(os.Args[4], config.Bytes(), 0644)
		if err != nil {
			fmt.Println(err)
		}
	} else {
		startServer(serverPort, config.Bytes())
	}
}

func startServer(portNumber int, config []byte) {
	fmt.Printf("Starting server on port %v\n", serverPort)
	address := net.UDPAddr{Port: portNumber}
	conn, err := net.ListenUDP("udp", &address)
	if err != nil {
		panic(err)
	}
	defer conn.Close()

	buffer := make([]byte, 10)
	for {
		var n int
		var addr net.Addr
		n, addr, err = conn.ReadFrom(buffer)
		if err != nil {
			fmt.Println(err)
		}
		if n == 1 || buffer[0] == 0 {
			go udpSend(conn, addr, config)
		} else {
			fmt.Printf("Response from %v: %v %v\n", addr.String(), n, string(buffer[:n]))
		}
	}
}
