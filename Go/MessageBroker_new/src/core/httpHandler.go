package core

import (
	"io/ioutil"
	"log"
	"net/http"
	"strings"
	"time"
)

type queueHandler struct {
	q *queue
}

func (q queueHandler) ServeHTTP(_ http.ResponseWriter, request *http.Request) {
	if request.Method != "POST" {
		log.Printf("invalid http method: %v", request.Method)
		return
	}
	idx := strings.LastIndex(request.URL.Path, "/")
	if idx == -1 {
		log.Printf("invalid request path: %v", request.URL.RawPath)
		return
	}
	sensorName := request.URL.Path[idx+1:]
	if len(sensorName) == 0 {
		log.Printf("sensor name is missing in request path: %v", request.URL.RawPath)
		return
	}

	body, err := ioutil.ReadAll(request.Body)
	if err != nil {
		log.Printf("request body read error: %v", err.Error())
	}
	if body == nil || len(body) == 0 {
		log.Println("unmarshal: empty body")
		return
	}

	data := string(body)

	logRequestBody(sensorName + " " + data)
	q.q.enQueue(SensorDataMessage{truncateToMinute(time.Now()), sensorName, data})
}

func truncateToMinute(t time.Time) time.Time {
	return time.Date(t.Year(), t.Month(), t.Day(), t.Hour(), t.Minute(), 0, 0, time.UTC)
}
func logRequestBody(requestBody string) {
	log.Printf("Request body: %v\n", requestBody)
}
