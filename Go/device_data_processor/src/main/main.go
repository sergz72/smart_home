package main

import (
    "core"
    "fmt"
    "os"
)

func main() {
    var iniFileName string

    switch len(os.Args) {
    case 1:
        iniFileName = "device_data_processor_config.json"
    case 2:
        iniFileName = os.Args[1]
    default:
        fmt.Println("Usage: device_data_processor [ini_file_name]")
        return
    }

    core.Init(iniFileName)
}
