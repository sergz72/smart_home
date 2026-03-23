use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use chrono_tz::Tz;
use redis::Client;
use crate::db::{get_current_date_time, Database, DatabaseParameters};
use crate::entities::{DeviceSensor, Location, MessageDateTime, Messages, Sensor, SensorTimestamp};

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
    fn insert_messages_to_db(&self, messages: Vec<Messages>) -> Result<(), Error> {
        let mut connection = self.client.get_connection()
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        for msg in messages {
            let timestamp = msg.date_time.to_millis(&self.tz);
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
        Ok(())
    }

    fn get_sensors(&self) -> Result<HashMap<usize, Sensor>, Error> {
        Ok(self.sensors.clone())
    }

    fn get_locations(&self) -> Result<HashMap<usize, Location>, Error> {
        Ok(self.locations.clone())
    }

    fn get_sensor_timestamps(&self) -> Result<HashMap<String, SensorTimestamp>, Error> {
        todo!()
    }

    fn get_current_date_time(&self) -> MessageDateTime {
        get_current_date_time(&self.tz)
    }

    fn build_device_sensors(&self) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error> {
        todo!()
    }
}
