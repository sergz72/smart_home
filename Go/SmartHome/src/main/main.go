package main

import (
	"core"
	"database/sql"
	"fmt"
	_ "github.com/lib/pq"
	"os"
	"strconv"
)

var db *sql.DB

func main() {
	if len(os.Args) != 6 {
		fmt.Println("Usage: SmartHome hostName userName password dbName portNumber")
		os.Exit(1)
	}
	dbinfo := fmt.Sprintf("host=%s user=%s password=%s dbname=%s sslmode=disable", os.Args[1], os.Args[2], os.Args[3], os.Args[4])
	var err error
	db, err = sql.Open("postgres", dbinfo)
	checkErr(err)
	defer db.Close()

	var portNumber int
	portNumber, err = strconv.Atoi(os.Args[5])

	err = core.ServerStart(db, portNumber)
	checkErr(err)
}

func checkErr(err error) {
	if err != nil {
		panic(err)
	}
}
