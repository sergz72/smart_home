package core

import (
    "bufio"
    "fmt"
    "github.com/fsnotify/fsnotify"
    "log"
    "os"
    "time"
)

func Init(iniFileName string) {
    if !loadConfiguration(iniFileName) {
        return
    }

    file, err := os.Open(config.LogFileName)
    if err != nil {
        log.Fatal(err)
    }
    defer file.Close()

    scanner := bufio.NewScanner(file)
    for scanner.Scan() {
    }
    if err := scanner.Err(); err != nil {
        log.Fatal(err)
    }

    // creates a new file watcher
    watcher, err := fsnotify.NewWatcher()
    if err != nil {
        log.Fatal(err)
    }
    defer watcher.Close()

    // out of the box fsnotify can watch a single file, or a single directory
    if err := watcher.Add(config.LogFileName); err != nil {
        log.Fatal(err)
    }

    go startWatcher(watcher, file)
    startCommandInterface()
}

func startWatcher(watcher *fsnotify.Watcher, file *os.File) {
    var l logAnalyzer

    l.init()

    fmt.Printf("Watching %v\n", config.LogFileName);
    for {
        select {
        // watch for events
        case event := <-watcher.Events:
            switch event.Op {
            case fsnotify.Write:
                scanner := bufio.NewScanner(file)
                for scanner.Scan() {
                    l.processLine(scanner.Text())
                }
                if err := scanner.Err(); err != nil {
                    log.Fatal(err)
                }
            case fsnotify.Create:
                fmt.Println("Log file create event.")
                var err error
                file, err = os.Open(config.LogFileName)
                if err != nil {
                    log.Fatal(err)
                }
                if err := watcher.Remove(config.LogFileName); err != nil {
                    log.Fatal(err)
                }
                if err := watcher.Add(config.LogFileName); err != nil {
                    log.Fatal(err)
                }
            case fsnotify.Rename:
                fmt.Println("Log file rename event.")
                var err error
                tries := 10
                for tries > 0 {
                    file, err = os.Open(config.LogFileName)
                    if err == nil {
                        fmt.Println("Log file re-opened.")
                        break
                    }
                    time.Sleep(1000)
                    tries--
                }
                if err != nil {
                    log.Fatal(err)
                }
                if err := watcher.Remove(config.LogFileName); err != nil {
                    log.Fatal(err)
                }
                if err := watcher.Add(config.LogFileName); err != nil {
                    log.Fatal(err)
                }
            case fsnotify.Remove:
                fmt.Println("Log file remove event.")
                file.Close()
            }

            // watch for errors
        case err := <-watcher.Errors:
            fmt.Printf("Watcher error: %v\n", err)
        }
    }
}