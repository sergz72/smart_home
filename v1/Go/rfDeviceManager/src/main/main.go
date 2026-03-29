package main

import (
    "core"
    "fmt"
    "log"
    "os"
    "strconv"
)

func main() {
    l := len(os.Args)
    if l < 2 || l > 3 {
        fmt.Println("Usage: nRF24 iniFileName [test_ext_temp]")
        os.Exit(1)
    }
    var testExtTemp float64 = -999;
    if l == 3 {
        var err error
        testExtTemp, err = strconv.ParseFloat(os.Args[2], 64)
        if err != nil {
            log.Fatal("incorrect test_ext_temp value")
        }
    }
    core.Init(os.Args[1], testExtTemp)
    core.Loop()
}
