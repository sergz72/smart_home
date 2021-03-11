package core

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io/ioutil"
	"log"
	"net"
	"strings"
	"sync"
	"time"
)

type Server struct {
	key []byte
	conn *net.UDPConn
	compressionType int
	db *DB
	config *configuration
	fetcherTimestamp map[string]int64
	mutex sync.Mutex
}

func ServerStart(_compressionType int, db *DB, config *configuration) error {
	key, err := ioutil.ReadFile(config.KeyFileName)
	if err != nil {
		return err
	}

	fetchKey, err := ioutil.ReadFile(config.FetchConfiguration.KeyFileName)
	if err != nil {
		return err
	}

	addr := net.UDPAddr{Port: config.PortNumber}
	conn, err := net.ListenUDP("udp", &addr)
	if err != nil {
		return err
	}
	defer conn.Close()

	err = conn.SetWriteBuffer(65507)
	if err != nil {
		return err
	}

	server := Server{key, conn, _compressionType, db, config,
		make(map[string]int64), sync.Mutex{} }

	fmt.Printf("Server started on port %d\n", config.PortNumber)

	go fetcherInit(&server, fetchKey)

	go TimerTask(&server, fetchKey)

	buffer := make([]byte, 10000)
	for {
		var n int
		var addr net.Addr
		n, addr, err = conn.ReadFrom(buffer)
		if err != nil {
			return err
		}
		go handle(&server, addr, buffer[:n])
	}
}

func logError(errorMessage string) {
	log.Println(errorMessage)
}

func logRequest(addr net.Addr) {
	log.Printf("Incoming request from address %s\n", addr.String())
}

func logRequestBody(requestBody string) {
	log.Printf("Request body: %s\n", requestBody)
}

func handle(server *Server, addr net.Addr, data []byte) {
	logRequest(addr)
	decodedData, err := AesDecode(data, server.key, CompressNone, func(nonce []byte) error {
		timePart := binary.LittleEndian.Uint64(nonce[len(nonce)-8:]) >> 16
		millis := uint64(time.Now().UnixNano() / 1000000)
		if (timePart >= millis && (timePart - millis < 60000)) ||
			(timePart < millis && (millis - timePart < 60000)) { // 60 seconds
			return nil
		}
		return fmt.Errorf("incorrect nonce")
	})
	if err != nil {
		logError(err.Error())
		return
	}
	var writer bytes.Buffer
	command := string(decodedData)
	logRequestBody(command)
	var errorMessage string
	if strings.HasPrefix(command, "GET ") {
		command = command[4:]
		if strings.HasPrefix(command, "/sensor_data?") {
			SensorDataHandler(server, &writer, command[13:])
		} else {
			errorMessage = "Invalid GET operation"
		}
	} else {
		errorMessage = fmt.Sprintf("Invalid method string: %v", command[:7])
	}
	if len(errorMessage) > 0 {
		logError(errorMessage)
		writer.Write([]byte(errorMessage))
	}
	encodedData, err := AesEncode(writer.Bytes(), server.key, server.compressionType, nil)
	if err != nil {
		logError(err.Error())
		_, _ = server.conn.WriteTo([]byte(err.Error()), addr)
	} else {
		_, _ = server.conn.WriteTo(encodedData, addr)
	}
}
