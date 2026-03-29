package core

import (
    "encoding/binary"
    "fmt"
    "net"
    "time"
)

func udpSend(key []byte, address string, command string, timeout time.Duration) ([]byte, error) {
    data, err := AesEncode([]byte(command), key, CompressNone, func(length int) ([]byte, error) {
        if length < 8 {
            return nil, fmt.Errorf("nonce size must be >= 8")
        }
        millis := uint64(time.Now().UnixNano() / 1000000)
        bMillis := make([]byte, 8)
        binary.LittleEndian.PutUint64(bMillis, millis)
        randPart, err := GenerateRandomBytes(length - 6)
        if err != nil {
            return nil, err
        }
        return append(randPart, bMillis[0], bMillis[1], bMillis[2], bMillis[3], bMillis[4], bMillis[5]), nil
    })
    if err != nil {
        return nil, err
    }
    raddr, err := net.ResolveUDPAddr("udp", address)
    if err != nil {
        return nil, err
    }

    var conn *net.UDPConn
    conn, err = net.DialUDP("udp", nil, raddr)
    if err != nil {
        return nil, err
    }
    defer conn.Close()
    err = conn.SetReadBuffer(65507)
    if err != nil {
        return nil, err
    }
    _, err = conn.Write(data)
    if err != nil {
        return nil, err
    }
    p := make([]byte, 65507)
    _ = conn.SetReadDeadline(time.Now().Add(timeout))
    var n int
    n, err = conn.Read(p)
    if err != nil {
        return nil, err
    }
    if n > 0 {
        fmt.Printf("Response length: %v\n", n)
        decodedData, err := AesDecode(p[:n], key, CompressGzip, nil)
        if err != nil {
            return nil, err
        }
        fmt.Printf("Decoded data length: %v\n", len(decodedData))
        fmt.Println(string(decodedData))
        return decodedData, nil
    } else {
        return nil, fmt.Errorf("zero length response")
    }
}
