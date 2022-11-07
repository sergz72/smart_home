package core

import (
	"fmt"
	"log"
	"net"
)

func tcpServerStart(server *Server) error {
	addr := net.TCPAddr{Port: server.config.TcpPortNumber}
	l, err := net.ListenTCP("tcp", &addr)
	if err != nil {
		return err
	}
	fmt.Printf("TCP server started on port %d\n", server.config.TcpPortNumber)
	go func() {
		defer l.Close()
		for {
			conn, err := l.Accept()
			if err != nil {
				log.Println("Error accepting: ", err.Error())
				return
			}
			// Handle connections in a new goroutine.
			go handleTcp(conn, server)
		}
	}()
	return nil
}

func logTcpRequest(addr net.Addr) {
	log.Printf("Incoming TCP request from address %s\n", addr.String())
}

func handleTcp(conn net.Conn, server *Server) {
	logTcpRequest(conn.RemoteAddr())
	buf := make([]byte, 1024)
	reqLen, err := conn.Read(buf)
	_ = conn.Close()
	if err != nil {
		return
	}
	if !tryLoadSensorData(server, buf[:reqLen], server.config.TimeOffset, true) {
		log.Println("Invalid sensor data")
	}
}
