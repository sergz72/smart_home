package main

import (
    "fmt"
    "os"
    "pumpControl/src/core"
)

func main() {
    l := len(os.Args)
    if l != 2 {
        fmt.Println("Usage: pumpControl iniFileName")
        os.Exit(1)
    }
    err := core.Loop(os.Args[1])
    if err != nil {
        panic(err)
    }
}
