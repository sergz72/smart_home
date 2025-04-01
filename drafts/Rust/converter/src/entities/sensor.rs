use std::collections::HashMap;
use std::fmt::Debug;
use std::fs::File;
use std::io::{BufReader, Error, ErrorKind};
use std::path::{Path, PathBuf};
use postgres::Client;
use serde::Deserialize;

#[derive(Debug, Deserialize, Clone)]
#[allow(dead_code)]
struct Sensor {
    #[serde(rename = "Id")]
    pub id: usize,
    #[serde(rename = "Name")]
    pub name: String,
    #[serde(rename = "DataType")]
    pub data_type: String,
    #[serde(rename = "LocationId")]
    pub location_id: usize,
    #[serde(rename = "DeviceId")]
    pub device_id: Option<usize>,
    #[serde(rename = "DeviceSensors", default)]
    pub device_sensors: HashMap<usize, String>,
    #[serde(rename = "Offsets", default)]
    pub offsets: HashMap<String, isize>,
}

fn read_sensors_from_json(file_name: PathBuf) -> Result<Vec<Sensor>, Error> {
    let file = File::open(file_name)?;
    let reader = BufReader::new(file);
    let sensors: Vec<Sensor> = serde_json::from_reader(reader)?;
    Ok(sensors)
}

fn build_device_sensors(sensors: HashMap<usize, String>) -> String {
    if sensors.is_empty() {
        "NULL".to_string()
    } else {
        let mut s = "ARRAY[".to_string();
        let mut first = true;
        for (id, value_type) in sensors {
            if first {
                first = false;
            } else {
                s += ",";
            }
            s += "'(";
            s += id.to_string().as_str();
            s += ",\"";
            s += value_type.as_str();
            s += "\")'::device_sensor";
        }
        s += "]";
        s
    }
}

fn build_offsets(offsets: HashMap<String, isize>) -> String {
    if offsets.is_empty() {
        "NULL".to_string()
    } else {
        let mut s = "ARRAY[".to_string();
        let mut first = true;
        for (value_type, offset) in offsets {
            if first {
                first = false;
            } else {
                s += ",";
            }
            s += "'(\"";
            s += value_type.as_str();
            s += "\",";
            s += offset.to_string().as_str();
            s += ")'::sensor_offset";
        }
        s += "]";
        s
    }
}

pub fn convert_sensors(mut client: Client, folder_name: &String) -> Result<(), Error> {
    let sensors = read_sensors_from_json(Path::new(folder_name).join("sensors.json"))?;
    for sensor in sensors {
        let id = sensor.id as i16;
        let location_id = sensor.location_id as i16;
        let device_sensors = build_device_sensors(sensor.device_sensors);
        let offsets = build_offsets(sensor.offsets);
        let device_id = sensor.device_id.map(|id| id as i16);
        let sql = "insert into sensors(id, name, data_type, location_id, device_id, device_sensors, offsets)\
                    values($1, $2, $3, $4, $5,".to_string() + device_sensors.as_str() + "," + offsets.as_str() + ")";
        client.execute(sql.as_str(),
                       &[&id, &sensor.name, &sensor.data_type, &location_id, &device_id])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
    }
    Ok(())
}
