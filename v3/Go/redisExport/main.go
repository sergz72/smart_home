package main

import (
	"errors"
	"fmt"
	"os"
	"strings"
	"time"
)

type database interface {
	getMaxTimestamp() (int64, error)
	importData(rows redisExport) error
}

func main() {
	parameters, err := parseArguments()
	if err != nil {
		panic(err)
	}
	timezone, ok := parameters["--timezone"]
	if !ok {
		panic("--timezone is required")
	}
	loc, err := time.LoadLocation(timezone)
	if err != nil {
		panic(err)
	}
	destination, err := buildDestinationDatabase(parameters, loc)
	if err != nil {
		panic(err)
	}
	redis, err := buildRedisDatabase(parameters, loc)
	if err != nil {
		panic(err)
	}
	maxTimestamp, err := destination.getMaxTimestamp()
	if err != nil {
		panic(err)
	}
	maxTimestamp++
	fmt.Printf("max timestamp: %d\n", maxTimestamp)
	rows, err := redis.exportData(maxTimestamp)
	if err != nil {
		panic(err)
	}
	err = destination.importData(rows)
	if err != nil {
		panic(err)
	}
	fmt.Println("Done")
}

func buildDestinationDatabase(parameters map[string]string, loc *time.Location) (database, error) {
	destination, ok := parameters["--destination"]
	if !ok {
		return nil, errors.New("--destination is required")
	}
	switch destination {
	case "postgres":
		return buildPostgresDatabase(parameters, loc)
	case "astra":
		return buildAstraDatabase(parameters, loc)
	default:
		return nil, errors.New("unknown destination")
	}
}

func parseArguments() (map[string]string, error) {
	parameters := make(map[string]string)
	for i := 1; i < len(os.Args); i++ {
		arg := os.Args[i]
		if strings.HasPrefix(arg, "--") && i < len(os.Args)-1 {
			i++
			parameters[arg] = os.Args[i]
		} else {
			usage()
		}
	}
	if len(parameters) == 0 {
		usage()
	}
	return parameters, nil
}

func usage() {
	fmt.Println("Usage: redisExport --redis_host_name value --timezone value --destination astra|postgres destination_parameters")
	os.Exit(1)
}
