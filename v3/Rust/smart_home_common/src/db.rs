use std::collections::HashMap;
use std::fs::File;
use std::io::{BufReader, Error, ErrorKind};
use chrono::Utc;
use chrono_tz::Tz;
use serde::Deserialize;
use crate::db_postgres::PostgresDatabase;
use crate::db_redis::RedisDatabase;
use crate::entities::{DeviceSensor, LastSensorData, Location, MessageDateTime, Messages, Sensor, SensorDataQuery, SensorDataResult, SensorTimestamp};

pub trait Database {
    fn insert_messages_to_db(&self, messages: Vec<Messages>) -> Result<(), Error>;
    fn get_sensors(&self) -> Result<HashMap<usize, Sensor>, Error>;
    fn get_locations(&self) -> Result<HashMap<usize, Location>, Error>;
    fn get_sensor_timestamps(&self) -> Result<HashMap<String, SensorTimestamp>, Error>;
    fn get_current_date_time(&self) -> MessageDateTime;
    fn build_device_sensors(&self) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error>;
    fn get_last_sensor_data(&self) -> Result<LastSensorData, Error>;
    fn get_sensor_data(&self, query: SensorDataQuery) -> Result<SensorDataResult, Error>;
    fn get_time_zone(&self) -> String;
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
    let naive_date = dt.naive_local();
    MessageDateTime{date_time: naive_date,
                    timestamp: Some(dt.timestamp_millis())}
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
        return Ok(PostgresDatabase::new(parameters.connection_string, parameters.tz)?)
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

#[cfg(test)]
mod tests {
    use std::io::{Error, ErrorKind};
    use chrono::{Datelike, NaiveDateTime, TimeZone, Timelike};
    use chrono_tz::Tz;
    use crate::db::Database;
    use crate::db_postgres::PostgresDatabase;

    #[test]
    fn test_datetime() -> Result<(), Error> {
        let tz: Tz = "Europe/Kyiv".parse().unwrap();
        let dt = NaiveDateTime::parse_from_str("2021-09-21 19:38:57", "%Y-%m-%d %H:%M:%S")
            .map_err(|e|Error::new(ErrorKind::Other, format!("error parsing datetime: {}", e)))?;
        let dt_tz = tz.from_local_datetime(&dt).unwrap();
        let year = dt_tz.year();
        let month = dt_tz.month();
        let day = dt_tz.day();
        let hour = dt_tz.hour();
        let minute = dt_tz.minute();
        let second = dt_tz.second();
        assert_eq!(year, 2021);
        assert_eq!(month, 9);
        assert_eq!(day, 21);
        assert_eq!(hour, 19);
        assert_eq!(minute, 38);
        assert_eq!(second, 57);
        assert_eq!(dt_tz.timestamp_millis(), 1632242337000); // 2021-09-21 16:38:57 GMT
        let naive_date = dt_tz.naive_local();
        assert_eq!(naive_date.year(), 2021);
        assert_eq!(naive_date.month(), 9);
        assert_eq!(naive_date.day(), 21);
        assert_eq!(naive_date.hour(), 19);
        assert_eq!(naive_date.minute(), 38);
        assert_eq!(naive_date.second(), 57);
        Ok(())
    }

    #[test]
    fn test_time_zone() -> Result<(), Error> {
        let db = PostgresDatabase::new("postgresql://postgres@localhost/smart_home".to_string(),
                                       "Europe/Kyiv".to_string().parse().map_err(|e|Error::new(ErrorKind::Other, e))?)?;
        assert_eq!(db.get_time_zone(), "Europe/Kyiv");
        Ok(())
    }
}