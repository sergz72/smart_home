package core

import (
	"net/http"
	"strconv"
)

func Run(configFileName string) error {
	config, err := loadConfiguration(configFileName)
	if err != nil {
		return err
	}

	var q queue
	q.initQueue(config)
	qh := queueHandler{&q}

	err = UDPServerStart(&q)
	if err != nil {
		return err
	}

	http.Handle("/sensor_data/", qh)
	return http.ListenAndServe(":" + strconv.Itoa(config.InPort), nil)
}
