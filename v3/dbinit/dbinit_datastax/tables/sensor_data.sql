CREATE TABLE IF NOT EXISTS sensor_data (
    sensor_id tinyint,
    datetime timestamp,
    values map<text, int>,
    PRIMARY KEY (sensor_id, datetime)
) WITH CLUSTERING ORDER BY (datetime DESC);
