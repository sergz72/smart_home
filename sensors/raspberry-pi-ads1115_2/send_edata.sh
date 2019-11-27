#! /bin/sh

/home/pi/sensors/ads1115/raspberry-pi-ads1115/get_vcc.sh | curl -d @- http://192.168.1.1:60000/sensor_data/garderob_ele
