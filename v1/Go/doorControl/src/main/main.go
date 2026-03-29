package main

import (
    "doorControl/src/core"
    "fmt"
    "os"
)

func main() {
    l := len(os.Args)
    if l != 2 {
        fmt.Println("Usage: doorControl iniFileName")
        os.Exit(1)
    }
    err := core.Loop(os.Args[1])
    if err != nil {
        panic(err)
    }
}
