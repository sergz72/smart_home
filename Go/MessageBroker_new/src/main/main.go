package main

import (
  "core"
  "fmt"
  "log"
  "os"
)

func main() {
  if len(os.Args) != 2 {
    fmt.Println("Usage: MessageBroker_new configFileName")
    os.Exit(1)
  }

  err := core.Run(os.Args[1])
  if err != nil {
    log.Fatal(err)
  }
}
