package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path"
	"strconv"
	"strings"
	"sync"
	"time"

	gocqlastra "github.com/datastax/gocql-astra"
	"github.com/gocql/gocql"
)

type tokens struct {
	ClientId string
	Secret   string
	Token    string
}

type sensorData struct {
	timestamp time.Time
	values    map[string]int32
}

func main() {
	if len(os.Args) < 4 {
		fmt.Println("Usage: dataStaxTest config_files_path keyspace command [parameters]")
		os.Exit(1)
	}
	configFilesPath := os.Args[1]
	fmt.Printf("Config files path: %s\n", configFilesPath)
	keyspace := os.Args[2]
	fmt.Printf("keyspace: %s\n", keyspace)
	command := os.Args[3]
	fmt.Printf("Command: %s\n", command)
	files, err := os.ReadDir(configFilesPath)
	if err != nil {
		panic(err)
	}
	var bundleFile string
	var tokenFile string
	for _, file := range files {
		if strings.HasSuffix(file.Name(), ".zip") {
			bundleFile = path.Join(configFilesPath, file.Name())
		}
		if strings.HasSuffix(file.Name(), ".json") {
			tokenFile = path.Join(configFilesPath, file.Name())
		}
		if bundleFile != "" && tokenFile != "" {
			break
		}
	}
	if bundleFile == "" {
		fmt.Println("No bundle file found")
		os.Exit(1)
	}
	if tokenFile == "" {
		fmt.Println("No token file found")
		os.Exit(1)
	}

	fmt.Printf("Bundle file: %s\n", bundleFile)
	fmt.Printf("Token file: %s\n", tokenFile)

	tokenFileContents, err := os.ReadFile(tokenFile)
	if err != nil {
		panic(err)
	}
	var t tokens
	err = json.Unmarshal(tokenFileContents, &t)
	if err != nil {
		panic(err)
	}

	cluster, err := gocqlastra.NewClusterFromBundle(bundleFile,
		"token", t.Token, 5*time.Second)
	if err != nil {
		panic(err)
	}
	cluster.Keyspace = keyspace

	start := time.Now()
	session, err := gocql.NewSession(*cluster)
	if err != nil {
		panic(err)
	}
	fmt.Printf("Connection process took %v\n", time.Since(start))

	switch command {
	case "last_data":
		fallthrough
	case "last_days_data":
		if len(os.Args) < 6 {
			fmt.Println("days and sensor ids expected")
			os.Exit(1)
		}
		days, err := strconv.Atoi(os.Args[4])
		if err != nil {
			panic(err)
		}
		sensorIds := os.Args[5:]
		sensorIdsInt := make([]byte, len(sensorIds))
		for i, sensorId := range sensorIds {
			sid, err := strconv.Atoi(sensorId)
			if err != nil {
				panic(err)
			}
			sensorIdsInt[i] = byte(sid)
		}
		minDateTime := time.Now().AddDate(0, 0, -days)
		if command == "last_data" {
			start = time.Now()
			lastData, err := fetchLastData(session, sensorIdsInt, minDateTime)
			if err != nil {
				panic(err)
			}
			fmt.Printf("Last data fetch took %v\n", time.Since(start))
			for sensorId, sensorLastData := range lastData {
				fmt.Printf("Sensor %d data %v\n", sensorId, sensorLastData)
			}
		} else {
			start = time.Now()
			data, errs := fetchLastDaysData(session, sensorIdsInt, minDateTime)
			fmt.Printf("Data fetch took %v\n", time.Since(start))
			for _, sensorId := range sensorIdsInt {
				e := errs[sensorId]
				if e == nil {
					fmt.Printf("Sensor %d row count %d\n", sensorId, len(data[sensorId]))
				} else {
					fmt.Printf("Sensor %d error %v\n", sensorId, e)
				}
			}
		}
		break
	default:
		fmt.Printf("Unknown command %s\n", command)
		os.Exit(1)
	}
}

func getIter(session *gocql.Session, sensorIds []byte, minDateTime time.Time) *gocql.Iter {
	var parameters []interface{}
	for _, sensorId := range sensorIds {
		parameters = append(parameters, sensorId)
	}
	parameters = append(parameters, minDateTime)
	return session.
		Query("select sensor_id, datetime, values from sensor_data where sensor_id in ("+
			strings.Repeat("?,", len(sensorIds)-1)+"?) and datetime > ? per partition limit 1", parameters...).
		Iter()
}

func fetchLastData(session *gocql.Session, sensorIds []byte, minDateTime time.Time) (map[int]sensorData, error) {
	result := make(map[int]sensorData)
	iter := getIter(session, sensorIds, minDateTime)
	var data sensorData
	var sensorId int
	for iter.Scan(&sensorId, &data.timestamp, &data.values) {
		result[sensorId] = data
	}
	err := iter.Close()
	return result, err
}

func fetchLastDaysData(session *gocql.Session, sensorIds []byte, minDateTime time.Time) ([][]sensorData, []error) {
	var maxSensorId byte
	for _, sensorId := range sensorIds {
		if maxSensorId < sensorId {
			maxSensorId = sensorId
		}
	}
	result := make([][]sensorData, maxSensorId+1)
	errs := make([]error, maxSensorId+1)
	var wg sync.WaitGroup
	for _, sensorId := range sensorIds {
		wg.Add(1)
		go func() {
			iter := session.
				Query("select datetime, values from sensor_data where sensor_id = ? and datetime > ?", sensorId, minDateTime).
				Iter()
			var data sensorData
			var sd []sensorData
			for iter.Scan(&data.timestamp, &data.values) {
				sd = append(sd, data)
			}
			result[sensorId] = sd
			errs[sensorId] = iter.Close()
			wg.Done()
		}()
	}
	wg.Wait()
	return result, errs
}
