package widget

import (
    "core"
    "fmt"
    "time"
)

func Clock() {
    for {
        t := time.Now()
        core.Queue <- core.DisplayMessage{CommandType: 1, Show: core.ShowDataCommand{ DeviceName: "clock",
                                          Messages: []core.TextLine{{Color: 16, Text: fmt.Sprintf("%02d:%02d", t.Hour(), t.Minute())}}}}
        time.Sleep(time.Second)
        t = time.Now()
        core.Queue <- core.DisplayMessage{CommandType: 1, Show: core.ShowDataCommand{ DeviceName: "clock",
                                          Messages: []core.TextLine{{ Color: 16, Text: fmt.Sprintf("%02d %02d", t.Hour(), t.Minute())}}}}
        time.Sleep(time.Second)
    }
}

