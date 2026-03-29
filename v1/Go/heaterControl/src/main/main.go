package main

import (
    "fmt"
    "heaterControl/src/core"
    "os"
)

func main() {
    l := len(os.Args)
    if l != 2 {
        fmt.Println("Usage: heaterControl iniFileName")
        os.Exit(1)
    }
    err := core.Loop(os.Args[1])
    if err != nil {
        panic(err)
    }
}
