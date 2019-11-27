#! /bin/sh

/home/pi/sensors/bme280/raspberry-pi-bme280/bme280 | curl -d @- http://192.168.1.1:60000/sensor_data/cabinet_int
