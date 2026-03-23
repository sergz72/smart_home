use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use chrono_tz::Tz;
use postgres::{Client, NoTls};
use crate::db::{get_current_date_time, Database};
use crate::entities::{Location, Messages, Sensor, SensorTimestamp};

pub struct PostgresDatabase {
    client: Client,
    tz: Tz
}

impl PostgresDatabase {
    pub fn new(connection_string: &String, tz: Tz) -> Result<Box<PostgresDatabase>, Error> {
        let client = Client::connect(connection_string, NoTls)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        Ok(Box::new(PostgresDatabase{client, tz}))
    }
}

impl Database for PostgresDatabase {
    fn insert_messages_to_db(&mut self, messages: Vec<Messages>) -> Result<(), Error> {
        for msg in messages {
            let date_time = msg.date_time.unwrap_or(get_current_date_time(&self.tz));
            for message in msg.messages {
                self.client.execute(
                    "INSERT INTO sensor_events(sensor_id, event_date, event_time, value_type, value)\
                       VALUES ($1, $2, $3, $4, $5)",
                    &[&msg.sensor_id, &date_time.date, &date_time.time, &message.value_type,
                        &message.value])
                    .map_err(|e| Error::new(ErrorKind::Other, e))?;
            }
        }
        Ok(())
    }

    fn get_sensors(&mut self) -> Result<HashMap<usize, Sensor>, Error> {
        todo!()
    }

    fn get_locations(&mut self) -> Result<HashMap<usize, Location>, Error> {
        todo!()
    }

    fn get_sensor_timestamps(&mut self) -> Result<HashMap<String, SensorTimestamp>, Error> {
        let rows = self.client.query("select * from v_sensors_latest_timestamp", &[])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
        let result: HashMap<String, SensorTimestamp> = rows.iter()
            .map(|row| (row.get(0), SensorTimestamp{id: row.get(1), last_timestamp: row.get(2)}))
            .collect();
        Ok(result)
    }
}
