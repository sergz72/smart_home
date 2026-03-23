use std::collections::HashMap;
use std::fs;
use std::fs::File;
use std::io::{Error, ErrorKind, Read, Write};
use std::path::{Path, PathBuf};
use postgres::Client;
use serde::Deserialize;

#[derive(Deserialize)]
struct SensorData {
    #[serde(rename = "EventTime")]
    pub event_time: usize,
    #[serde(rename = "Data")]
    pub data: HashMap<String, i32>
}

struct SensorDataDay {
    date: usize,
    // map sensor id to sensor data
    data: HashMap<usize, Vec<SensorData>>
}

fn read_sensor_data_from_string(data: String) -> Result<Vec<SensorData>, Error> {
    let sensors: Vec<SensorData> = serde_json::from_str(&data)?;
    Ok(sensors)
}

fn read_sensor_data_from_folder(folder_name: PathBuf) -> Result<Vec<SensorDataDay>, Error> {
    let folder_files = fs::read_dir(&folder_name)?;
    let mut result = Vec::new();
    for folder_file in folder_files {
        let f = &folder_file?;
        if f.file_type()?.is_dir() {
            let date_folder_path = f.path();
            let date = f.file_name().into_string().unwrap().parse::<usize>()
                .map_err(|e|Error::new(ErrorKind::InvalidData, e))?;
            let date_files = fs::read_dir(&date_folder_path)?;
            let mut map = HashMap::new();
            for date_file in date_files {
                let pdate_file = &date_file?;
                let pdate_file_path = pdate_file.path();
                let pdate_file_name = pdate_file_path.file_stem().unwrap();
                let sensor_id = pdate_file_name.to_str().unwrap().to_string().parse::<usize>()
                        .map_err(|e|Error::new(ErrorKind::InvalidData, e))?;
                let mut ff = File::open(pdate_file.path())?;
                let mut contents = String::new();
                ff.read_to_string(&mut contents)?;
                let sensor_data = read_sensor_data_from_string(contents)?;
                map.insert(sensor_id, sensor_data);
            }
            result.push(SensorDataDay{date, data: map});
        }
    }
    Ok(result)
}

fn read_sensor_data_from_zip(file_name: PathBuf) -> Result<Vec<SensorDataDay>, Error> {
    let mut result = HashMap::new();
    let file = File::open(file_name)?;
    let mut archive = zip::ZipArchive::new(file)?;
    for i in 0..archive.len() {
        let mut file = archive.by_index(i)?;
        if file.is_file() {
            let file_name = file.name().to_owned();
            let parts = file_name.split('/').collect::<Vec<&str>>();
            if parts.len() < 2 {
                return Err(Error::new(ErrorKind::InvalidData, "Invalid filename: ".to_string() + file_name.as_str()));
            }
            let date = parts[parts.len() - 2].parse::<usize>()
                .map_err(|e|Error::new(ErrorKind::InvalidData, e))?;
            let sensor_file_name = Path::new(parts[parts.len() - 1]).file_stem().unwrap().to_str().unwrap();
            let sensor_id = sensor_file_name.parse::<usize>()
                .map_err(|e|Error::new(ErrorKind::InvalidData, e))?;
            let mut contents = String::new();
            file.read_to_string(&mut contents)?;
            let sensor_data = read_sensor_data_from_string(contents)?;
            let day = result
                .entry(date)
                .or_insert(HashMap::new());
            day.insert(sensor_id, sensor_data);
        }
    }
    Ok(result.into_iter().map(|(k, v)|SensorDataDay{date: k, data: v}).collect())
}

fn insert_sensor_data(client: &mut Client, sensor_data: Vec<SensorDataDay>, file_name: &str,
                      date: i32) -> Result<(), Error> {
    let mut events_file = File::create(file_name)?;
    let mut exists = false;
    for day in sensor_data.into_iter().filter(|d| d.date >= date as usize) {
        exists = true;
        for (sensor_id, sensor_data_vec) in &day.data {
            for sensor_data in sensor_data_vec {
                for (value_type, value) in &sensor_data.data {
                    events_file.write_fmt(format_args!("{},{},{},{},{}\n", *sensor_id, day.date,
                                                       sensor_data.event_time, value_type, *value))?;
                }
            }
        }
    }
    if exists {
        let statement =
            "copy sensor_events(sensor_id, event_date, event_time, value_type, value) from '".to_string()
                + file_name + "' DELIMITER ',' CSV";
        client.execute(&statement, &[])
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
    }
    Ok(())
}

fn reset_table_and_sequence(client: &mut Client, date: i32) -> Result<(), Error> {
    if date != 0 {
        client.execute("DELETE FROM sensor_events where event_date >= $1", &[&date])
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
    } else {
        client.execute("TRUNCATE TABLE sensor_events", &[])
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        client.execute("SELECT setval('sensor_events_id_seq', 1)", &[])
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
    }
    Ok(())
}

pub fn convert_sensor_data(mut client: Client, folder_name: &String, from: Option<&String>)
    -> Result<(), Error> {
    let mut date = 0;
    if let Some(from) = from {
        date = from.parse::<i32>().map_err(|e| Error::new(ErrorKind::Other, e))?;
    }
    let file_data =
        read_sensor_data_from_folder(Path::new(folder_name).join("dates_new"))?;
    let zip_data =
        read_sensor_data_from_zip(Path::new(folder_name).join("db.zip"))?;
    reset_table_and_sequence(&mut client, date)?;
    insert_sensor_data(&mut client, file_data, "/tmp/sensor_events1.csv", date)?;
    insert_sensor_data(&mut client, zip_data, "/tmp/sensor_events2.csv", date)
}
