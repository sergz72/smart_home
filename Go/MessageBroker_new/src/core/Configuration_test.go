package core

import "testing"

func TestConfigurationLoad(t *testing.T) {
    config, err := loadConfiguration("../../test_resources/testConfiguration.json")
    if err != nil {
        t.Fatal(err)
    }
    if config.KeyFileName != "key.dat" {
        t.Fatal("incorrect key file name")
    }
    if config.InPort != 60000 {
        t.Fatal("incorrect InPort value")
    }
    if config.OutPort != 60001 {
        t.Fatal("incorrect OutPort value")
    }
    if config.QueueParameters.Size != 1000 {
        t.Fatal("incorrect QueueParameters.Size value")
    }
    if config.QueueParameters.MaxResponseSize != 100 {
        t.Fatal("incorrect QueueParameters.MaxResponseSize value")
    }
    if config.QueueParameters.expirationTime != 60 * 60 {
        t.Fatal("incorrect QueueParameters.expirationTime value")
    }
}

func TestQueueConfigurationLoad(t *testing.T) {
    config, err := loadConfiguration("../../test_resources/queueTestConfiguration.json")
    if err != nil {
        t.Fatal(err)
    }
    if config.KeyFileName != "key2.dat" {
        t.Fatal("incorrect key file name")
    }
    if config.InPort != 60001 {
        t.Fatal("incorrect InPort value")
    }
    if config.OutPort != 60002 {
        t.Fatal("incorrect OutPort value")
    }
    if config.QueueParameters.Size != 5 {
        t.Fatal("incorrect QueueParameters.Size value")
    }
    if config.QueueParameters.MaxResponseSize != 3 {
        t.Fatal("incorrect QueueParameters.MaxResponseSize value")
    }
    if config.QueueParameters.expirationTime != 120 {
        t.Fatal("incorrect QueueParameters.expirationTime value")
    }
}

func TestConfigurationLoad2(t *testing.T) {
    config, err := loadConfiguration("../../test_resources/configuration.json")
    if err != nil {
        t.Fatal(err)
    }
    if config.KeyFileName != "key.dat" {
        t.Fatal("incorrect key file name")
    }
    if config.InPort != 60000 {
        t.Fatal("incorrect InPort value")
    }
    if config.OutPort != 60001 {
        t.Fatal("incorrect OutPort value")
    }
    if config.QueueParameters.Size != 10000 {
        t.Fatal("incorrect QueueParameters.Size value")
    }
    if config.QueueParameters.MaxResponseSize != 100 {
        t.Fatal("incorrect QueueParameters.MaxResponseSize value")
    }
    if config.QueueParameters.expirationTime != 2 * 60 * 60 * 24 {
        t.Fatal("incorrect QueueParameters.expirationTime value")
    }
}
