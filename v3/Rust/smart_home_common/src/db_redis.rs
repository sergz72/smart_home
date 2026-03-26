use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use std::ops::Sub;
use chrono::{Days, Utc};
use chrono_tz::Tz;
use redis::Client;
use crate::db::{build_device_sensors, get_current_date_time, get_date_and_time, Database, DatabaseParameters};
use crate::entities::{DeviceSensor, LastSensorData, Location, MessageDateTime, Messages, Sensor, SensorDataItem, SensorDataQuery, SensorDataResult, SensorTimestamp};
use crate::logger::Logger;

pub struct RedisDatabase {
    client: Client,
    tz: Tz,
    sensors: HashMap<usize, Sensor>,
    locations: HashMap<usize, Location>
}

impl RedisDatabase {
    pub fn new(parameters: DatabaseParameters) -> Result<Box<RedisDatabase>, Error> {
        let client = Client::open(parameters.connection_string.clone())
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let sensors = parameters.sensors.ok_or(Error::new(ErrorKind::Other, "Sensors not provided"))?;
        let locations = parameters.locations.ok_or(Error::new(ErrorKind::Other, "Locations not provided"))?;
        Ok(Box::new(RedisDatabase{client, tz: parameters.tz, sensors, locations}))
    }
}

impl Database for RedisDatabase {
    fn insert_messages_to_db(&self, messages: Vec<Messages>, logger: &Logger, dry_run: bool) -> Result<(), Error> {
        let mut connection = self.client.get_connection()
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        for msg in messages {
            let timestamp_opt = msg.date_time.to_timestamp_millis(&self.tz);
            if let Some(timestamp) = timestamp_opt {
                for message in msg.messages {
                    let value_type = format!("{:4}", message.value_type);
                    let series_name = format!("{}:{}", msg.sensor_id, value_type);
                    let value = message.value as f64 / 100.0;
                    if dry_run {
                        logger.info(&format!("TS.ADD \"{}\" {} {}", series_name, timestamp, value));
                        continue;
                    }
                    let _: () = redis::cmd("TS.ADD")
                        .arg(series_name)
                        .arg(timestamp)
                        .arg(value)
                        .query(&mut connection)
                        .map_err(|e| Error::new(ErrorKind::Other, e))?;
                }
            } else {
                logger.error(&format!("Cannot convert date time to timestamp: {}", msg.date_time));
            }
        }
        Ok(())
    }

    fn get_sensors(&self) -> Result<HashMap<usize, Sensor>, Error> {
        Ok(self.sensors.clone())
    }

    fn get_locations(&self) -> Result<HashMap<usize, Location>, Error> {
        Ok(self.locations.clone())
    }

    fn get_sensor_timestamps(&self) -> Result<HashMap<String, SensorTimestamp>, Error> {
        let mut connection = self.client.get_connection()
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let results: Vec<(String, Vec<(String, String)>, Vec<(u64, f64)>)> = redis::cmd("TS.MGET")
            .arg("FILTER")
            .arg("type=sensor")
            .query(&mut connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let mut result = HashMap::new();
        for (series_name, _, values) in results {
            let parts: Vec<&str> = series_name.split(':').collect();
            let sensor_id = parts[0].parse::<usize>()
                .map_err(|_|Error::new(ErrorKind::InvalidData, format!("Invalid sensor id {}", parts[0])))?;
            let sensor_name = self.sensors[&sensor_id].name.clone();
            for (timestamp, _) in values {
                let timestamp_i64 = timestamp as i64;
                let ts = result
                    .entry(sensor_name.clone())
                    .or_insert(SensorTimestamp{id: sensor_id as i16, last_timestamp: 0});
                if timestamp_i64 > ts.last_timestamp {
                    ts.last_timestamp = timestamp_i64;
                }
            }
        }
        Ok(result)
    }

    fn get_current_date_time(&self) -> MessageDateTime {
        get_current_date_time(&self.tz)
    }

    fn get_date_and_time(&self, timestamp: i64) -> Result<(i32, i32), Error> {
        get_date_and_time(timestamp, self.tz)
    }
    
    fn build_device_sensors(&self) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error> {
        Ok(build_device_sensors(&self.sensors))
    }

    fn get_last_sensor_data(&self) -> Result<LastSensorData, Error> {
        let min_timestamp = Utc::now().sub(Days::new(1)).timestamp_millis() as u64;
        let mut connection = self.client.get_connection()
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let results: Vec<(String, Vec<(String, String)>, Vec<(u64, f64)>)> = redis::cmd("TS.MGET")
            .arg("FILTER")
            .arg("type=sensor")
            .query(&mut connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let mut result = HashMap::new();
        for (series_name, _, values) in results {
            let parts: Vec<&str> = series_name.split(':').collect();
            let sensor_id = parts[0].parse::<usize>()
                .map_err(|_|Error::new(ErrorKind::InvalidData, format!("Invalid sensor id {}", parts[0])))?;
            let location_id = self.sensors[&sensor_id].location_id;
            let value_type = parts[1].to_string();
            for (timestamp, value) in values {
                if timestamp < min_timestamp {
                    continue;
                }
                let location_values = result.entry(location_id).or_insert(HashMap::new());
                location_values.insert(value_type.clone(), SensorDataItem{timestamp: timestamp as i64, value: (value * 100.0) as i32});
            }
        }
        Ok(LastSensorData::new(result))
    }

    fn get_sensor_data(&self, query: SensorDataQuery) -> Result<SensorDataResult, Error> {
        todo!()
    }

    fn get_time_zone(&self) -> String {
        self.tz.name().to_string()
    }
}

#[cfg(test)]
mod tests {
    use std::io::Error;
    use crate::db::{create_database_from_configuration, DatabaseConfiguration};

    #[test]
    fn test_get_sensor_timestamps() -> Result<(), Error> {
        let db = create_database_from_configuration(&DatabaseConfiguration{
            connection_string: "redis://127.0.0.1".to_string(),
            sensors_file: Some("test_resources/sensors1.json".to_string()),
            locations_file: Some("test_resources/locations1.json".to_string()),
            time_zone: "Europe/Berlin".to_string(),
        })?;
        let timestamps = db.get_sensor_timestamps()?;
        assert_eq!(timestamps.len(), 9);
        Ok(())
    }

    #[test]
    fn test_get_last_sensor_data() -> Result<(), Error> {
        let db = create_database_from_configuration(&DatabaseConfiguration{
            connection_string: "redis://127.0.0.1".to_string(),
            sensors_file: Some("test_resources/sensors1.json".to_string()),
            locations_file: Some("test_resources/locations1.json".to_string()),
            time_zone: "Europe/Berlin".to_string(),
        })?;
        let data = db.get_last_sensor_data()?;
        assert_eq!(data.data.len(), 6);
        Ok(())
    }
}