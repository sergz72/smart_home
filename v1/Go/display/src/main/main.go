package main

import (
    "core"
    "fmt"
    "github.com/kataras/iris"
    "os"
    "strconv"
    "widget"

    "github.com/kataras/iris/middleware/logger"
    "github.com/kataras/iris/middleware/recover"
)

func main() {
    var iniFileName string

    switch len(os.Args) {
    case 2:
        iniFileName = "display_config.json"
    case 3:
        iniFileName = os.Args[2]
    default:
        fmt.Println("Usage: display deviceName [ini_file_name]")
        os.Exit(1)
    }

    core.LoadConfiguration(iniFileName)

    fmt.Println("Display init...")
    err := core.DisplayInit(os.Args[1])
    if err != nil {
        panic(err)
    }
    fmt.Println("done.")

    core.MessageProcessorInit()

    go core.MessageProcessor()
    go widget.Clock()
    if len(core.Config.SmartHomeAddress) > 0 {
        go widget.Env()
    }
    if core.Config.Max44009 != nil {
        go widget.Max44009()
    }

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

    app.Post("/show", core.ShowDataHandler)
    app.Post("/display", core.DisplayCommandHandler)

    app.Run(iris.Addr(":" + strconv.Itoa(core.Config.Port)), iris.WithoutServerError(iris.ErrServerClosed))
}
