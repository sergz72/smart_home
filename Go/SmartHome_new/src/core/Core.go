package core

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func Run(iniFileName string) error {
	config, err := loadConfiguration(iniFileName)
	if err != nil {
		return err
	}

	fmt.Println("Reading DB files...")
	start := time.Now()
	db := DB{}
	err = db.Load(config, start)
	if err != nil {
		return err
	}
	fmt.Printf("%v elapsed.\n", time.Since(start))

	//handle CTRL C
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-c
		fmt.Print("Interrupt signal. Sending timer stop event...")
		TimerTaskStopChannel <- true
	}()

	var compressionType int
	compressionType, err = GetCompressionType(config.CompressionType)
	if err != nil {
		return err
	}

	return ServerStart(compressionType, &db, config)
}

