package core

import "testing"

func TestConfigurationLoad(t *testing.T) {
	config, err := loadConfiguration("../../test_resources/testConfiguration.json")
	if err != nil {
		t.Fatal(err)
	}
	if config.DataFolder != "testFolder" {
		t.Fatal("incorrect DataFolder")
	}
	if config.KeyFileName != "key.dat" {
		t.Fatal("incorrect key file name")
	}
	if config.PortNumber != 59999 {
		t.Fatal("incorrect PortNumber value")
	}
	if config.CompressionType != "bz" {
		t.Fatal("incorrect CompressionType value")
	}
	if config.BackupInterval != 60 * 60 {
		t.Fatal("incorrect BackupInterval value")
	}
	if config.AggregationHour != 6 {
		t.Fatal("incorrect AggregationHour value")
	}
	if config.RawDataDays != 30 {
		t.Fatal("incorrect RawDataDays value")
	}
	if config.FetchConfiguration.KeyFileName != "fetchKey.dat" {
		t.Fatal("incorrect FetchConfiguration.KeyFileName value")
	}
	if config.FetchConfiguration.Interval != 5 * 60 {
		t.Fatal("incorrect FetchConfiguration.Interval value")
	}
	if config.FetchConfiguration.Timeout != 10 {
		t.Fatal("incorrect FetchConfiguration.Timeout value")
	}
	if config.FetchConfiguration.Retries != 3 {
		t.Fatal("incorrect FetchConfiguration.Retries value")
	}
	if len(config.FetchConfiguration.Addresses) != 3 {
		t.Fatal("incorrect FetchConfiguration.Addresses length")
	}
}

