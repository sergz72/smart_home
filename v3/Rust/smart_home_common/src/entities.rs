use std::collections::HashMap;
use std::fmt::{Display, Formatter};
use chrono::{MappedLocalTime, TimeZone};
use chrono_tz::Tz;
use serde::Deserialize;

#[derive(Debug)]
pub struct MessageDateTime {
    pub date: i32,
    pub time: i32
}

impl MessageDateTime {
    pub fn to_millis(&self, tz: &Tz) -> Option<i64> {
        match tz.with_ymd_and_hms(
            self.date / 10000,
            ((self.date / 100) % 100) as u32,
            (self.date % 100) as u32,
            (self.time / 10000) as u32,
            ((self.time / 100) % 100) as u32,
            (self.time % 100) as u32) {
            MappedLocalTime::Single(dt) => Some(dt.timestamp_millis()),
            MappedLocalTime::Ambiguous(_, latest) => Some(latest.timestamp_millis()),
            MappedLocalTime::None => None
        }
    }
}

pub struct Message {
    pub value_type: String,
    pub value: i32
}

impl Display for Message {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "message value_type={} value={}", self.value_type, self.value)
    }
}

pub struct Messages {
    pub messages: Vec<Message>,
    pub sensor_id: i16,
    pub date_time: Option<MessageDateTime>
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
    #[serde(rename = "DeviceSensors")]
    pub device_sensors: HashMap<usize, String>,
    #[serde(rename = "Offsets")]
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

pub struct SensorTimestamp {
    pub id: i16,
    pub last_timestamp: i64
}
