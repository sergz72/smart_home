package main

import (
	"database/sql"
	"fmt"
	_ "github.com/lib/pq"
	"os"

	"github.com/kataras/iris"

	"github.com/kataras/iris/middleware/logger"
	"github.com/kataras/iris/middleware/recover"
)

func main() {
	if len(os.Args) != 5 {
		fmt.Println("Usage: SensorData hostName userName password dbName")
		os.Exit(1)
	}
	dbinfo := fmt.Sprintf("host=%s user=%s password=%s dbname=%s sslmode=disable", os.Args[1], os.Args[2], os.Args[3], os.Args[4])
	db, err := sql.Open("postgres", dbinfo)
	checkErr(err)
	defer db.Close()
    
    go SensorDataProcessor(db)
    
	app := iris.New()
	app.Logger().SetLevel("debug")
	// Optionally, add two built'n handlers
	// that can recover from any http-relative panics
	// and log the requests to the terminal.
	app.Use(recover.New())
	app.Use(logger.New())

    app.OnErrorCode(iris.StatusNotFound, func(ctx iris.Context) {
        ctx.HTML("<b>Resource Not found</b>")
    })

    app.Post("/sensor_data/{sensor_name}", SensorDataHandler)

	app.Run(iris.Addr(":60000"), iris.WithoutServerError(iris.ErrServerClosed))
}

func checkErr(err error) {
	if err != nil {
		panic(err)
	}
}
