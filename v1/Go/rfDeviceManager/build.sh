#! /bin/sh

export GOROOT=/home/pi/go
export GOPATH=/home/pi/rfDeviceManager
/home/pi/go/bin/go build -i -o /home/pi/rfDeviceManager/bin/rfDeviceManager main
