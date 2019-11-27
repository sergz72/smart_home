package core

import (
    "github.com/kataras/iris"
    "github.com/kataras/iris/middleware/logger"
    "github.com/kataras/iris/middleware/recover"
    "io/ioutil"
    "strconv"
)

func startCommandInterface() {
    app := iris.New()
    app.Logger().SetLevel(config.LogLevel)
    // Optionally, add two built'n handlers
    // that can recover from any http-relative panics
    // and log the requests to the terminal.
    app.Use(recover.New())
    app.Use(logger.New())

    app.OnErrorCode(iris.StatusNotFound, func(ctx iris.Context) {
        ctx.HTML("<b>Resource Not found</b>")
    })

    app.Post("/command/{deviceName}", commandHandler)

    app.Run(iris.Addr(":" + strconv.Itoa(config.Port)), iris.WithoutServerError(iris.ErrServerClosed))
}

func commandHandler(ctx iris.Context) {
    if ctx.Request().Body == nil {
        ctx.WriteString("Unmarshal: nil body")
        return
    }
    rawData, err := ioutil.ReadAll(ctx.Request().Body)
    if err != nil {
        ctx.Writef("Unmarshal: read error: %v", err)
        return
    }
    if len(rawData) == 0 {
        ctx.WriteString("Unmarshal: empty body")
        return
    }

    deviceName := ctx.Params().Get("deviceName")
    d := findDeviceByName(deviceName)
    if d == nil {
        ctx.Writef("Unknown device: %v", deviceName)
        return
    }
    if !d.active {
        ctx.Writef("Device %v is inactive.", deviceName)
        return
    }
    ctx.Write(d.handler.CommandHandler(rawData))
}

