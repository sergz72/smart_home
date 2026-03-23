use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use std::sync::Mutex;
use chrono_tz::Tz;
use postgres::{Client, NoTls};
use crate::db::{get_current_date_time, Database};
use crate::entities::{DeviceSensor, Location, MessageDateTime, Messages, Sensor, SensorTimestamp};

pub struct PostgresDatabase {
    client: Mutex<Client>,
    tz: Tz
}

impl PostgresDatabase {
    pub fn new(connection_string: &String, tz: Tz) -> Result<Box<PostgresDatabase>, Error> {
        let client = Client::connect(connection_string, NoTls)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        Ok(Box::new(PostgresDatabase{client: Mutex::new(client), tz}))
    }
}

impl Database for PostgresDatabase {
    fn insert_messages_to_db(&self, messages: Vec<Messages>) -> Result<(), Error> {
        for msg in messages {
            for message in msg.messages {
                self.client.lock().unwrap().execute(
                    "INSERT INTO sensor_events(sensor_id, event_date, event_time, value_type, value)\
                       VALUES ($1, $2, $3, $4, $5)",
                    &[&msg.sensor_id, &msg.date_time.date, &msg.date_time.time, &message.value_type,
                        &message.value])
                    .map_err(|e| Error::new(ErrorKind::Other, e))?;
            }
        }
        Ok(())
    }

    fn get_sensors(&self) -> Result<HashMap<usize, Sensor>, Error> {
        /*let sql = "select s.id, s.data_type, l.name, l.location_type from sensors s, locations l
where s.location_id = l.id";
        self.client.lock().unwrap().query(sql, &[])
            .map_err(|e| Error::new(ErrorKind::Other, e))*/
        todo!()
    }

    fn get_locations(&self) -> Result<HashMap<usize, Location>, Error> {
        todo!()
    }

    fn get_sensor_timestamps(&self) -> Result<HashMap<String, SensorTimestamp>, Error> {
        let rows = self.client.lock().unwrap().query("select * from v_sensors_latest_timestamp", &[])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
        let result: HashMap<String, SensorTimestamp> = rows.iter()
            .map(|row| (row.get(0), SensorTimestamp{id: row.get(1), last_timestamp: row.get(2)}))
            .collect();
        Ok(result)
    }

    fn get_current_date_time(&self) -> MessageDateTime {
        get_current_date_time(&self.tz)
    }

    // map device id to vector of DeviceSensor
    fn build_device_sensors(&self) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error> {
        let result = self.client.lock().unwrap().query("
with s as (
select id, device_id, unnest(device_sensors) dsensors
from sensors
where device_id is not null and enabled = true
), so as (
select id, device_id, unnest(offsets) doffsets
from sensors
where device_id is not null and enabled = true
)
select s.id, s.device_id, (s.dsensors).id idx, (s.dsensors).value_type, coalesce((so.doffsets).offset_value, 0) offset_value
from s left join so
  on so.id = s.id and so.device_id = s.device_id and (s.dsensors).value_type = (so.doffsets).value_type", &[])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
        let mut sensor_map = HashMap::new();
        for row in result {
            let sensor_id: i16 = row.get(0);
            let device_id: i16 = row.get(1);
            let sensor_idx: i16 = row.get(2);
            let value_type: String = row.get(3);
            let offset_value: i32 = row.get(4);
            let map = sensor_map
                .entry(device_id as usize)
                .or_insert(HashMap::new());
            map.insert(sensor_idx as usize,DeviceSensor{sensor_id, value_type, offset_value});
        }
        Ok(sensor_map)
    }
}

#[cfg(test)]
mod tests {
    use std::io::Error;
    use crate::db::{create_database_from_configuration, DatabaseConfiguration};

    #[test]
    fn test_load_device_sensors() -> Result<(), Error> {
        let db = create_database_from_configuration(&DatabaseConfiguration{
            connection_string: "postgresql://postgres@localhost/smart_home".to_string(),
            sensors_file: None,
            locations_file: None,
            time_zone: "Europe/Berlin".to_string(),
        })?;
        let sensors = db.build_device_sensors()?;
        assert_eq!(sensors.len(), 5);
        assert_eq!(sensors.contains_key(&1), true);
        let sensors1 = sensors.get(&1).unwrap();
        assert_eq!(sensors1.len(), 7);
        assert_eq!(sensors1.contains_key(&4), true);
        let sensors14 = sensors1.get(&4).unwrap();
        assert_eq!(sensors14.sensor_id, 6);
        assert_eq!(sensors14.value_type, "humi");
        Ok(())
    }
}
