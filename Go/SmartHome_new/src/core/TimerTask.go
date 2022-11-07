package core

import (
    "fmt"
    "os"
    "time"
)

var TimerTaskStopChannel = make(chan bool)

func TimerTask(server *Server, fetchKey []byte) {
    seconds := 0
    hour := time.Now().Hour()
    for {
        time.Sleep(time.Second)
        now := time.Now()
        select {
        case _ = <-TimerTaskStopChannel:
            fmt.Print("Timer stop event. Exiting...")
            server.db.backupData(server.config, now)
            os.Exit(0)
        default:
            break
        }
        seconds++
        if server.config.FetchConfiguration.Interval > 0 && (seconds%server.config.FetchConfiguration.Interval == 0) {
            fetchData(server, fetchKey)
        }
        if (seconds % server.config.BackupInterval) == 0 {
            server.db.backupData(server.config, now)
        }
        hourNow := now.Hour()
        if hour != hourNow {
            hour = hourNow
            if hour == server.config.AggregationHour {
                server.db.aggregateLastDayData()
            }
        }
    }
}
