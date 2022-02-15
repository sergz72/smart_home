package core

import (
    "encoding/binary"
    "encoding/json"
    "fmt"
    "log"
    "net"
    "strings"
    "time"
)

func serverStart() error {
    addr := net.UDPAddr{Port: config.UdpPort}
    conn, err := net.ListenUDP("udp", &addr)
    if err != nil {
        return err
    }

    err = conn.SetWriteBuffer(65507)
    if err != nil {
        return err
    }

    fmt.Printf("UDP server started on port %d\n", config.UdpPort)

    go serverLoop(conn)

    return nil
}

func serverLoop(conn *net.UDPConn) {
    defer conn.Close()
    buffer := make([]byte, 10000)
    for {
        n, addr, err := conn.ReadFrom(buffer)
        if err != nil {
            logError(err.Error())
        } else {
            go handle(conn, addr, buffer[:n])
        }
    }
}

func logError(errorMessage string) {
    log.Println(errorMessage)
}

func logRequest(addr net.Addr) {
    log.Printf("Incoming request from address %s\n", addr.String())
}

func logRequestBody(requestBody string) {
    log.Printf("Request body: %v\n", requestBody)
}

func forceOffHandler() {
    pumpState.ForcedOff = true
    log.Println("forceOff")
}

func activateHandler() {
    pumpState.ForcedOff = false
    log.Println("activate")
}

func handle(conn *net.UDPConn, addr net.Addr, data []byte) {
    logRequest(addr)
    decodedData, err := AesDecode(data, config.key, false, func(nonce []byte) error {
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
    command := string(decodedData)
    logRequestBody(command)
    var errorMessage string
    if command == "GET /status" {
        status := buildPumpStatus()
        data, err := json.Marshal(status)
        if err != nil {
            errorMessage = err.Error()
        } else {
            _, _ = conn.WriteTo(data, addr)
            return
        }
    } else if strings.HasPrefix(command, "POST ") {
        command = command[5:]
        switch command {
        case "/forceOff":
            forceOffHandler()
        case "/activate":
            activateHandler()
        default:
            errorMessage = fmt.Sprintf("Bad request: %v", command)
        }
    } else {
        errorMessage = fmt.Sprintf("Invalid method: %v", command)
    }
    responseMessage := "Ok"
    if len(errorMessage) > 0 {
        logError(errorMessage)
        responseMessage = errorMessage
    }
    _, _ = conn.WriteTo([]byte(responseMessage), addr)
}
