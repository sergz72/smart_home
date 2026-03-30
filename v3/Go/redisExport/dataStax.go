package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path"
	"strings"
	"time"

	gocqlastra "github.com/datastax/gocql-astra"
	"github.com/gocql/gocql"
)

type tokens struct {
	ClientId string
	Secret   string
	Token    string
}

type astraDatabase struct {
	session *gocql.Session
	loc     *time.Location
}

func buildAstraDatabase(parameters map[string]string, loc *time.Location) (*astraDatabase, error) {
	configFilesPath, ok := parameters["--config_files_path"]
	if !ok {
		return &astraDatabase{}, errors.New("--config_files_path is required")
	}
	clusterConfig, err := buildAstraClusterConfig(configFilesPath)
	if err != nil {
		return &astraDatabase{}, err
	}
	keyspace, ok := parameters["--keyspace"]
	if !ok {
		return &astraDatabase{}, errors.New("--keyspace is required")
	}
	clusterConfig.Keyspace = keyspace
	session, err := gocql.NewSession(*clusterConfig)
	if err != nil {
		return &astraDatabase{}, err
	}
	return &astraDatabase{session, loc}, nil
}

func (d *astraDatabase) getMaxTimestamp() (int64, error) {
	iter := d.session.Query("select datetime from sensor_data per partition limit 1").Iter()
	var dt time.Time
	var maxTimestamp int64
	for iter.Scan(&dt) {
		newTime := time.Date(dt.Year(), dt.Month(), dt.Day(), dt.Hour(), dt.Minute(), dt.Second(), dt.Nanosecond(), d.loc)
		timestamp := newTime.UnixMilli()
		if timestamp > maxTimestamp {
			maxTimestamp = timestamp
		}
	}
	return maxTimestamp, iter.Close()
}

func (d *astraDatabase) importData(rows redisExport) error {
	transformed := rows.transformToSensorData()
	batch := d.session.NewBatch(gocql.LoggedBatch)
	l := 10
	for sensorId, data := range transformed {
		fmt.Printf("SensorId: %d row count %d\n", sensorId, len(data))
		for _, row := range data {
			newTime := time.Date(row.timestamp.Year(), row.timestamp.Month(), row.timestamp.Day(), row.timestamp.Hour(),
				row.timestamp.Minute(), row.timestamp.Second(), row.timestamp.Nanosecond(), time.UTC)
			batch.Query("insert into sensor_data (sensor_id, datetime, values) values (?, ?, ?)",
				sensorId, newTime, row.values)
			l--
			if l == 0 {
				err := d.session.ExecuteBatch(batch)
				if err != nil {
					return err
				}
				l = 10
				batch = d.session.NewBatch(gocql.LoggedBatch)
			}
		}
		if len(batch.Entries) != 0 {
			err := d.session.ExecuteBatch(batch)
			if err != nil {
				return err
			}
		}
	}
	return nil
}

func buildAstraClusterConfig(configFilesPath string) (*gocql.ClusterConfig, error) {
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
		return nil, errors.New("no bundle file found")
	}
	if tokenFile == "" {
		return nil, errors.New("no token file found")
	}

	tokenFileContents, err := os.ReadFile(tokenFile)
	if err != nil {
		panic(err)
	}

	var t tokens
	err = json.Unmarshal(tokenFileContents, &t)
	if err != nil {
		return nil, err
	}

	return gocqlastra.NewClusterFromBundle(bundleFile,
		"token", t.Token, 5*time.Second)
}
