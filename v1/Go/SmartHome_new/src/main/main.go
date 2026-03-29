package main

import (
	"fmt"
	"log"
	"os"
	"smartHome/src/core"
)

func main() {
	l := len(os.Args)
	if l != 2 {
		fmt.Println("Usage: SmartHome_new iniFileName")
		os.Exit(1)
	}

	err := core.Run(os.Args[1])
	if err != nil {
		log.Fatal(err)
	}
}
