use std::collections::HashMap;
use std::fmt::{Display, Formatter};
use std::io::{Error, ErrorKind};
use chrono::{DateTime, Datelike, Days, MappedLocalTime, Months, NaiveDateTime, TimeZone, Timelike, Utc};
use chrono_tz::Tz;
use serde::Deserialize;

#[derive(Debug, Clone)]
pub struct MessageDateTime {
    pub date_time: NaiveDateTime,
    pub timestamp: Option<i64>
}

impl Display for MessageDateTime {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.date_time.format("%Y-%m-%d %H:%M:%S"))
    }
}

impl MessageDateTime {
    pub fn to_date_time_tz(&self, tz: &Tz) -> Option<DateTime<Tz>> {
        match tz.from_local_datetime(&self.date_time) {
            MappedLocalTime::Single(dt) => Some(dt),
            MappedLocalTime::Ambiguous(_, latest) => Some(latest),
            MappedLocalTime::None => None
        }
    }

    pub fn to_timestamp_millis(&self, tz: &Tz) -> Option<i64> {
        self.timestamp.or(self.to_date_time_tz(tz).map(|dt| dt.timestamp_millis()))
    }

    pub fn to_date_and_time(&self) -> (i32, i32) {
        let date = self.date_time.year() * 10000 +
            (self.date_time.month() * 100) as i32 +
            self.date_time.day() as i32;
        let time = (self.date_time.hour() * 10000) as i32 +
            (self.date_time.minute() * 100) as i32 +
            self.date_time.second() as i32;
        (date, time)
    }
}

pub struct Message {
    pub value_type: String,
    pub value: i32
}

impl Message {
    pub fn to_string(&self) -> String {
        format!("{}={}", self.value_type, self.value)
    }
}

impl Display for Message {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "message value_type={} value={}", self.value_type, self.value)
    }
}

pub struct Messages {
    pub messages: Vec<Message>,
    pub sensor_id: i16,
    pub date_time: MessageDateTime
}

impl Messages {
    pub fn to_string(&self) -> String {
        format!("{} {} [{}]", self.date_time, self.sensor_id,
            self.messages.iter().map(|m| m.to_string()).collect::<Vec<String>>().join(", "))
    }
}

#[derive(Deserialize, Clone)]
pub struct Sensor {
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
    pub offsets: HashMap<String, f64>,
    #[serde(rename = "Enabled")]
    pub enabled: bool
}

#[derive(Deserialize, Clone)]
pub struct Location {
    #[serde(rename = "Name")]
    pub name: String,
    #[serde(rename = "LocationType")]
    pub location_type: String
}

impl Location {
    pub fn to_binary(&self, v: &mut Vec<u8>) -> Result<(), Error> {
        add_string_to_binary(v, &self.name);
        add_fixed_size_string_to_binary(v, &self.location_type, 3)
    }
}

pub struct SensorTimestamp {
    pub id: i16,
    pub last_timestamp: i64
}

#[derive(Debug)]
pub struct DeviceSensor {
    pub sensor_id: i16,
    pub value_type: String,
    pub offset_value: i32
}

pub struct SensorDataItem {
    pub timestamp: i64,
    pub value: i32
}

impl SensorDataItem {
    pub(crate) fn to_binary(&self, v: &mut Vec<u8>) {
        v.extend_from_slice(&((self.timestamp / 1000) as u32).to_le_bytes());
        v.extend_from_slice(&self.value.to_le_bytes());
    }
}

pub struct LastSensorData {
    pub data: HashMap<usize, HashMap<String, SensorDataItem>>
}

impl LastSensorData {
    pub fn new(data: HashMap<usize, HashMap<String, SensorDataItem>>) -> LastSensorData {
        LastSensorData{data}
    }

    pub fn to_binary(&self) -> Result<Vec<u8>, Error>{
        let mut v = Vec::new();
        for (location_id, last_data_by_location) in &self.data {
            v.push(*location_id as u8);
            v.push(last_data_by_location.len() as u8);
            for (value_type, last_data) in last_data_by_location {
                add_fixed_size_string_to_binary(&mut v, value_type, 4)?;
                last_data.to_binary(&mut v);
            }
        }
        Ok(v)
    }
}

pub struct AggregatedSensorData {
    pub min: Vec<SensorDataItem>,
    pub avg: Vec<SensorDataItem>,
    pub max: Vec<SensorDataItem>
}

impl AggregatedSensorData {
    pub fn to_binary(&self) -> Vec<u8> {
        todo!()
    }
}

pub enum SensorDataList {
    Raw(Vec<SensorDataItem>),
    Aggregated(AggregatedSensorData)
}

impl SensorDataList {
    pub fn to_binary(&self) -> Vec<u8> {
        todo!()
    }
}

pub struct SensorData {
    pub location_id: usize,
    pub data: SensorDataList
}

impl SensorData {
    pub fn to_binary(&self) -> Vec<u8> {
        todo!()
    }
}

pub struct SensorDataResult {
    data: HashMap<String, Vec<SensorData>>
}

impl SensorDataResult {
    pub fn to_binary(&self) -> Vec<u8> {
        todo!()
    }
}

pub struct SensorDataQuery {
    max_points: usize,
    data_type: String,
    date_or_offset: i32,
    offset_unit: i32,
    period: i32,
    period_unit: i32
}

impl SensorDataQuery {
    pub fn from_binary(command: Vec<u8>) -> Result<SensorDataQuery, Error> {
        let mut buffer16 = [0u8; 2];
        buffer16.copy_from_slice(&command[0..2]);
        let mut max_points = u16::from_le_bytes(buffer16) as usize;
        let data_type = String::from_utf8(command[2..5].to_vec())
            .map_err(|e| Error::new(ErrorKind::InvalidData, e))?;
        let mut buffer32 = [0u8; 4];
        buffer32.copy_from_slice(&command[5..9]);
        let mut date_or_offset = i32::from_le_bytes(buffer32);
        let period = command[9] as i32;
        let period_unit = command[10] as i32;
        let offset_unit = date_or_offset & 0xFF;
        if date_or_offset < 0 {
            date_or_offset >>= 8;
        }
        if max_points == 0 {
            max_points = 2000;
        }
        Ok(SensorDataQuery{max_points, data_type, date_or_offset, offset_unit, period, period_unit})
    }

    pub fn to_parameters(&self, tz: &Tz) -> Result<SensorDataQueryParameters, Error> {
        let now = Utc::now().with_timezone(tz);
        let start_date = if self.date_or_offset < 0 {
            parse_period(now, self.date_or_offset, self.offset_unit)?
        } else {
            match tz.with_ymd_and_hms(
                self.date_or_offset / 10000,
                ((self.date_or_offset / 100) % 100) as u32,
                (self.date_or_offset % 100) as u32, 0, 0, 0) {
                MappedLocalTime::Single(dt) => dt,
                MappedLocalTime::Ambiguous(_, latest) => latest,
                MappedLocalTime::None => return Err(Error::new(ErrorKind::InvalidData, "Invalid date_or_offset"))
            }
        };
        let end_date = if self.period != 0 {
            parse_period(start_date, self.period, self.period_unit)?
        } else { now };
        let aggregated = (end_date - start_date).num_hours() > self.max_points as i64;
        Ok(SensorDataQueryParameters{aggregated, max_points: self.max_points, data_type: self.data_type.clone(), start_date, end_date})
    }
}

fn parse_period(start_datetime: DateTime<Tz>, period: i32, period_unit: i32)
                -> Result<DateTime<Tz>, Error> {
    if period >= 0 {
        match period_unit {
            0 => Ok(start_datetime + Days::new(period as u64)),
            1 => Ok(start_datetime + Months::new(period as u32)),
            2 => Ok(start_datetime + Months::new((period * 12) as u32)),
            _ => Err(Error::new(ErrorKind::InvalidData, "Invalid period_unit"))
        }
    } else {
        match period_unit {
            0 => Ok(start_datetime - Days::new(period as u64)),
            1 => Ok(start_datetime - Months::new(-period as u32)),
            2 => Ok(start_datetime - Months::new((-period * 12) as u32)),
            _ => Err(Error::new(ErrorKind::InvalidData, "Invalid period_unit"))
        }
    }
}

pub struct SensorDataQueryParameters {
    pub aggregated: bool,
    pub max_points: usize,
    pub data_type: String,
    pub start_date: DateTime<Tz>,
    pub end_date: DateTime<Tz>
}

pub fn add_string_to_binary(binary: &mut Vec<u8>, string: &String) {
    binary.push(string.len() as u8);
    let bytes = string.as_bytes();
    binary.extend_from_slice(bytes);
}

pub fn add_fixed_size_string_to_binary(binary: &mut Vec<u8>, string: &String, expected_length: usize)
    -> Result<(), Error> {
    let bytes = string.as_bytes();
    if bytes.len() != expected_length {
        return Err(Error::new(ErrorKind::InvalidData, "invalid string length"));
    }
    binary.extend_from_slice(bytes);
    Ok(())
}