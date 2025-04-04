mod configuration;
mod message_processor;

use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use std::thread;
use smart_home_common::base_server::BaseServer;
use smart_home_common::db::DB;
use crate::configuration::load_configuration;
use crate::message_processor::build_message_processor;

#[derive(Debug)]
pub struct DeviceSensor {
    pub sensor_id: i16,
    pub value_type: String,
    pub offset_value: i32
}

// map device id to vector of DeviceSensor
fn load_device_sensors(db: &DB) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error> {
    let mut client = db.get_database_connection()?;
    let result = client.query("
with s as (
select id, device_id, unnest(device_sensors) dsensors
from sensors
where device_id is not null
), so as (
select id, device_id, unnest(offsets) doffsets
from sensors
where device_id is not null
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

fn main() -> Result<(), Error> {
    let ini_file_name = &std::env::args().nth(1).expect("no file name given");
    let config = load_configuration(ini_file_name)?;
    let db = DB::new(config.connection_string.clone());
    let device_sensors = load_device_sensors(&db)?;
    let message_processor = 
        build_message_processor(&config.device_key_file_name, device_sensors, db)?;
    if config.tcp_port_number != 0 {
        let pn = config.tcp_port_number;
        let processor = message_processor.clone();
        let tcp_server = Box::leak(Box::new(
            BaseServer::new(false, pn, processor, config.time_offset, "tcp_server".to_string())?));
        thread::spawn(move ||tcp_server.start());
    }
    let udp_server =
        Box::leak(Box::new(BaseServer::new(true, config.port_number,
                                           message_processor.clone(),
                                           config.time_offset, "udp_server".to_string())?));
    udp_server.start();
    Ok(())
}

#[cfg(test)]
mod tests {
    use smart_home_common::db::DB;
    use crate::load_device_sensors;

    #[test]
    fn test_load_device_sensors() {
        let db = DB::new("postgresql://postgres@localhost/smart_home".to_string());
        let sensors_result = load_device_sensors(&db);
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
