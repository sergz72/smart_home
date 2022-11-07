package core

import (
	"encoding/json"
	"fmt"
	"os"
	"time"
)

type fetchConfiguration struct {
	KeyFileName     string
	Interval        int
	Timeout         int
	Retries         int
	Addresses       []string
	timeoutDuration time.Duration
}

type configuration struct {
	DataFolder         string
	KeyFileName        string
	PortNumber         int
	TcpPortNumber      int
	CompressionType    string
	BackupInterval     int
	AggregationHour    int
	RawDataDays        int
	FetchConfiguration fetchConfiguration
	DeviceKeyFileName  string
	TimeOffset         int
	TotalCalculation   map[string]int
}

func loadConfiguration(iniFileName string) (*configuration, error) {
	iniFileContents, err := os.ReadFile(iniFileName)
	if err != nil {
		return nil, err
	}
	var config configuration
	err = json.Unmarshal(iniFileContents, &config)
	if err != nil {
		return nil, err
	}
	if len(config.DataFolder) == 0 || len(config.KeyFileName) == 0 || config.BackupInterval <= 0 || config.PortNumber <= 0 ||
		len(config.CompressionType) == 0 || config.AggregationHour <= 0 || config.RawDataDays <= 0 {
		return nil, fmt.Errorf("incorrect configuration parameters")
	}
	if config.FetchConfiguration.Interval > 0 || config.FetchConfiguration.Timeout > 0 ||
		len(config.FetchConfiguration.KeyFileName) > 0 || config.FetchConfiguration.Addresses != nil {
		if config.FetchConfiguration.Interval <= 0 || config.FetchConfiguration.Timeout <= 0 ||
			len(config.FetchConfiguration.KeyFileName) == 0 {
			return nil, fmt.Errorf("incorrect fetch configuration parameters")
		}
		if config.FetchConfiguration.Addresses == nil || len(config.FetchConfiguration.Addresses) == 0 {
			return nil, fmt.Errorf("no addresses to process messages from")
		}
		config.FetchConfiguration.Interval *= 60
		config.FetchConfiguration.timeoutDuration = time.Duration(config.FetchConfiguration.Timeout) * time.Second
	}

	config.BackupInterval *= 60

	return &config, nil
}
