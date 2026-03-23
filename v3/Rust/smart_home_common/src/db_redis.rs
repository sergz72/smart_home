use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use chrono::Utc;
use chrono_tz::Tz;
use redis::Client;
use crate::db::{Database, DatabaseParameters};
use crate::entities::{Location, Messages, Sensor, SensorTimestamp};

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
    fn insert_messages_to_db(&mut self, messages: Vec<Messages>) -> Result<(), Error> {
        let mut connection = self.client.get_connection()
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        for msg in messages {
            let timestamp_opt = msg.date_time
                .map(|dt| dt.to_millis(&self.tz))
                .unwrap_or(Some(Utc::now().timestamp_millis()));

            if let Some(timestamp) = timestamp_opt {
                for message in msg.messages {
                    let value_type = format!("{:4}", message.value_type);
                    let _: () = redis::cmd("TS.ADD")
                        .arg(format!("{}:{}", msg.sensor_id, value_type))
                        .arg(timestamp)
                        .arg(message.value as f64 / 100.0)
                        .query(&mut connection)
                        .map_err(|e| Error::new(ErrorKind::Other, e))?;
                }
            }
        }
        Ok(())
    }

    fn get_sensors(&mut self) -> Result<HashMap<usize, Sensor>, Error> {
        Ok(self.sensors.clone())
    }

    fn get_locations(&mut self) -> Result<HashMap<usize, Location>, Error> {
        Ok(self.locations.clone())
    }

    fn get_sensor_timestamps(&mut self) -> Result<HashMap<String, SensorTimestamp>, Error> {
        todo!()
    }
}
