package core

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io/ioutil"
	"log"
	"net"
	"strconv"
	"strings"
	"time"
)

type Server struct {
	key []byte
	conn *net.UDPConn
	q *queue
}

func UDPServerStart(q *queue) error {
	key, err := ioutil.ReadFile(q.config.KeyFileName)
	if err != nil {
		return err
	}

	addr := net.UDPAddr{Port: q.config.OutPort}
	conn, err := net.ListenUDP("udp", &addr)
	if err != nil {
		return err
	}

	err = conn.SetWriteBuffer(65507)
	if err != nil {
		return err
	}

	server := Server{key, conn, q}

	fmt.Printf("UDP server started on port %d\n", q.config.OutPort)

	go serverLoop(&server)

	return nil
}

func serverLoop(server *Server) {
	defer server.conn.Close()
	buffer := make([]byte, 10000)
	for {
		n, addr, err := server.conn.ReadFrom(buffer)
		if err != nil {
			log.Println(err)
		} else {
			go handle(server, addr, buffer[:n])
		}
	}
}

func logError(errorMessage string) {
	log.Println(errorMessage)
}

func logRequest(addr net.Addr) {
	log.Printf("Incoming request from address %s\n", addr.String())
}

func handle(server *Server, addr net.Addr, data []byte) {
	logRequest(addr)
	decodedData, err := AesDecode(data, server.key, false, func(nonce []byte) error {
		timePart := binary.LittleEndian.Uint64(nonce[len(nonce)-8:]) >> 16
		millis := uint64(time.Now().UnixNano() / 1000000)
		if (timePart >= millis && (timePart-millis < 60000)) ||
			(timePart < millis && (millis-timePart < 60000)) { // 60 seconds
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
		if strings.HasPrefix(command, "/sensor_data/") {
			parameter := command[13:]
			idx := strings.Index(parameter, "@")
			var from int64 = -1
			if idx != -1 {
				from, err = strconv.ParseInt(parameter[idx+1:], 10, 64)
				if err != nil {
					errorMessage = "Invalid from value"
				} else {
					parameter = parameter[:idx]
				}
			}
			if len(errorMessage) == 0 {
				//fmt.Printf("from = %v\n", from)
				if parameter == "all" {
					UploadSensorData(server, from, &writer)
				} else if parameter == "sensors" {
					server.q.mutex.Lock()
					writer.Write([]byte("["))
					first := true
					for k, _ := range server.q.lastValues {
						if first {
							first = false
						} else {
							writer.Write([]byte(","))
						}
						writer.Write([]byte("\"" + k + "\""))
					}
					writer.Write([]byte("]"))
					server.q.mutex.Unlock()
				} else {
					server.q.mutex.Lock()
					writer.Write([]byte(server.q.lastValues[parameter]))
					server.q.mutex.Unlock()
				}
			}
		} else {
			errorMessage = "Invalid GET operation"
		}
	} else {
		errorMessage = fmt.Sprintf("Invalid method string: %v", command[:7])
	}
	if len(errorMessage) > 0 {
		logError(errorMessage)
		writer.Write([]byte(errorMessage))
		return
	}
	encodedData, err := AesEncode(writer.Bytes(), server.key, true, nil)
	if err != nil {
		logError(err.Error())
		_, _ = server.conn.WriteTo([]byte(err.Error()), addr)
	} else {
		_, _ = server.conn.WriteTo(encodedData, addr)
	}
}

func UploadSensorData(server *Server, from int64, w *bytes.Buffer) {
	messages := server.q.deQueue(from)
	w.Write([]byte("["))
	sep := []byte(",")
	first := true
    for _, message := range messages {
		if first {
			first = false
		} else {
			w.Write(sep)
		}
		var messageString string
		if strings.HasPrefix(message.message, "{") {
			//valid response
			messageString = message.message
		} else {
			//error response
			messageString = "\"" + message.message + "\""
		}
		w.Write([]byte(fmt.Sprintf("{\"messageTime\": \"%v\", \"sensorName\": \"%v\", \"message\": %v}",
			message.messageTime.Format("2006-01-02 15:04"),
			message.sensorName,
			messageString)))
	}

	w.Write([]byte("]"))
}

