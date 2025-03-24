use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use postgres::{Client, NoTls};

pub struct DB {
    client: Client
}

#[derive(Debug)]
pub struct DeviceSensor {
    sensor_id: usize,
    value_type: String
}

fn get_database_connection(connection_string: &String) -> Result<Client, Error> {
    Client::connect(connection_string, NoTls)
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

impl DB {
    pub fn new(connection_string: &String) -> Result<DB, Error> {
        Ok(DB{client: get_database_connection(connection_string)?})
    }
    
    // map device id to vector of DeviceSensor
    pub fn load_device_sensors(&mut self) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error> {
        let result = self.client.query("
with s as (
select id, device_id, unnest(device_sensors) dsensors
  from sensors
 where device_id is not null
)
select s.id, s.device_id, (s.dsensors).id idx, (s.dsensors).value_type
  from s", &[])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
        let mut sensor_map = HashMap::new();
        for row in result {
            let sensor_id: i16 = row.get(0);
            let device_id: i16 = row.get(1);
            let sensor_idx: i16 = row.get(2);
            let value_type: String = row.get(3);
            let map = sensor_map
                .entry(device_id as usize)
                .or_insert(HashMap::new());
            map.insert(sensor_idx as usize,DeviceSensor{sensor_id: sensor_id as usize, value_type});
        }
        Ok(sensor_map)
    }
}

#[cfg(test)]
mod tests {
    use crate::db::DB;

    #[test]
    fn test_load_device_sensors() {
        let db_result = DB::new(&"postgresql://postgres@localhost/smart_home".to_string());
        assert!(!db_result.is_err(), "DB::new error");
        let sensors_result = db_result.unwrap().load_device_sensors();
        assert!(!sensors_result.is_err(), "load_device_sensors error {}", sensors_result.unwrap_err());
        let sensors = sensors_result.unwrap();
        assert_eq!(sensors.len(), 5);
        assert_eq!(sensors.contains_key(&1), true);
        let sensors1 = sensors.get(&1).unwrap();
        assert_eq!(sensors1.len(), 10);
        assert_eq!(sensors1.contains_key(&4), true);
        let sensors14 = sensors1.get(&4).unwrap();
        assert_eq!(sensors14.sensor_id, 6);
        assert_eq!(sensors14.value_type, "humi");
    }
}
