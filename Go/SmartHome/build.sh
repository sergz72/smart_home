#! /bin/sh

export GOPATH=`pwd`
/usr/local/go/bin/go install -v ./...
mv bin/main bin/SmartHome
