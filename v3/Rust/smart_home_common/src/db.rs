use std::collections::HashMap;
use std::fs::File;
use std::io::{BufReader, Error, ErrorKind};
use chrono::{Datelike, Timelike, Utc};
use chrono_tz::Tz;
use serde::Deserialize;
use crate::db_postgres::PostgresDatabase;
use crate::db_redis::RedisDatabase;
use crate::entities::{DeviceSensor, Location, MessageDateTime, Messages, Sensor, SensorTimestamp};

pub trait Database {
    fn insert_messages_to_db(&self, messages: Vec<Messages>) -> Result<(), Error>;
    fn get_sensors(&self) -> Result<HashMap<usize, Sensor>, Error>;
    fn get_locations(&self) -> Result<HashMap<usize, Location>, Error>;
    fn get_sensor_timestamps(&self) -> Result<HashMap<String, SensorTimestamp>, Error>;
    fn get_current_date_time(&self) -> MessageDateTime;
    fn build_device_sensors(&self) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error>;
}

#[derive(Debug, Deserialize)]
pub struct DatabaseConfiguration {
    #[serde(rename = "ConnectionString")]
    pub connection_string: String,
    #[serde(rename = "SensorsFile")]
    pub sensors_file: Option<String>,
    #[serde(rename = "LocationsFile")]
    pub locations_file: Option<String>,
    #[serde(rename = "TimeZone")]
    pub time_zone: String
}

pub struct DatabaseParameters {
    pub connection_string: String,
    pub tz: Tz,
    pub sensors: Option<HashMap<usize, Sensor>>,
    pub locations: Option<HashMap<usize, Location>>
}

impl DatabaseConfiguration {
    fn to_database_parameters(&self) -> Result<DatabaseParameters, Error> {
        let tz = self.time_zone.parse()
            .map_err(|e| Error::new(ErrorKind::InvalidInput, e))?;
        let sensors = if let Some(file_name) = &self.sensors_file {
            Some(build_sensors(file_name)?)
        } else {None};
        let locations = if let Some(file_name) = &self.locations_file {
            Some(build_locations(file_name)?)
        } else {None};
        Ok(DatabaseParameters{connection_string: self.connection_string.clone(), tz, sensors, locations})
    }

    pub fn validate(&self) -> Result<(), Error> {
        if self.connection_string.is_empty() {
            return Err(Error::new(ErrorKind::InvalidInput, "Connection string cannot be empty"))
        }
        if self.time_zone.is_empty() {
            return Err(Error::new(ErrorKind::InvalidInput, "Time zone cannot be empty"))
        }
        Ok(())
    }
}

pub fn get_current_date_time(tz: &Tz) -> MessageDateTime {
    let dt = Utc::now().with_timezone(tz);
    MessageDateTime{date: dt.year() * 10000 + (dt.month() * 100) as i32 + dt.day() as i32,
                    time: (dt.hour() * 10000) as i32 + (dt.minute() * 100) as i32 + dt.second() as i32}
}

pub fn create_database_from_configuration_file(file_name: &String) -> Result<Box<dyn Database + Send + Sync>, Error> {
    let file = File::open(file_name)?;
    let reader = BufReader::new(file);
    let configuration: DatabaseConfiguration = serde_json::from_reader(reader)?;
    create_database_from_configuration(&configuration)
}

pub fn create_database_from_configuration(configuration: &DatabaseConfiguration) -> Result<Box<dyn Database + Send + Sync>, Error> {
    configuration.to_database_parameters().and_then(|parameters| create_database_from_parameters(parameters))
}

pub fn create_database_from_parameters(parameters: DatabaseParameters) -> Result<Box<dyn Database + Send + Sync>, Error> {
    if parameters.connection_string.starts_with("postgresql://") {
        return Ok(PostgresDatabase::new(&parameters.connection_string, parameters.tz)?)
    }
    if parameters.connection_string.starts_with("redis://") {
        return Ok(RedisDatabase::new(parameters)?)
    }
    Err(Error::new(ErrorKind::InvalidInput, "Unknown database type"))
}

fn build_locations(file_name: &String) -> Result<HashMap<usize, Location>, Error> {
    let file = File::open(file_name)?;
    let reader = BufReader::new(file);
    let locations: HashMap<usize, Location> = serde_json::from_reader(reader)?;
    Ok(locations)
}

fn build_sensors(file_name: &String) -> Result<HashMap<usize, Sensor>, Error> {
    let file = File::open(file_name)?;
    let reader = BufReader::new(file);
    let sensors: HashMap<usize, Sensor> = serde_json::from_reader(reader)?;
    Ok(sensors)
}
