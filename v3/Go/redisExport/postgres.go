package main

import (
	"errors"
	"time"
)

type postgresDatabase struct {
	postgresHostName string
	postgresDbName   string
	loc              *time.Location
}

func buildPostgresDatabase(parameters map[string]string, loc *time.Location) (*postgresDatabase, error) {
	postgresHostName, ok := parameters["--postgres_host_name"]
	if !ok {
		postgresHostName = "127.0.0.1"
	}
	postgresDbName, ok := parameters["--postgres_db_name"]
	if !ok {
		return &postgresDatabase{}, errors.New("--postgres_db_name is required")
	}
	return &postgresDatabase{postgresHostName, postgresDbName, loc}, errors.New("not implemented")
}

func (d *postgresDatabase) getMaxTimestamp() (int64, error) {
	return 0, errors.New("not implemented")
}

func (d *postgresDatabase) importData(rows redisExport) error {
	return errors.New("not implemented")
}
