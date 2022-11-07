package entities

import (
    "encoding/json"
    "os"
)

type Location struct {
    Id           int
    Name         string
    LocationType string
}

func ReadLocationsFromJson(path string) (map[int]Location, error) {
    var locations []Location
    dat, err := os.ReadFile(path)
    if err != nil {
        return nil, err
    }
    if err := json.Unmarshal(dat, &locations); err != nil {
        return nil, err
    }

    result := make(map[int]Location)
    for _, l := range locations {
        result[l.Id] = l
    }

    return result, nil
}
