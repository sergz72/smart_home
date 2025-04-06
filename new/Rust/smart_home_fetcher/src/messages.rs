use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use chrono::{Datelike, NaiveDateTime, Timelike};
use serde::{Deserialize, Deserializer};
use serde_json::Value;
use smart_home_common::db::MessageWithDate;
use smart_home_common::logger::Logger;

pub struct Sensor {
    pub id: i16,
    pub last_timestamp: i64
}

#[allow(dead_code)]
struct Message {
    error_message: String,
    value_map: HashMap<String, f64>
}

#[derive(Deserialize)]
#[allow(dead_code)]
struct ResponseMessage {
    #[serde(deserialize_with = "datefmt", rename = "messageTime")]
    message_time: NaiveDateTime,
    #[serde(rename = "sensorName")]
    sensor_name: String,
    #[serde(deserialize_with = "map_or_error")]
    message: Message
}

fn datefmt<'de, D>(deserializer: D) -> Result<NaiveDateTime, D::Error> where D: Deserializer<'de>
{
    let time = String::deserialize(deserializer)?;
    NaiveDateTime::parse_from_str(&time, "%Y-%m-%d %H:%M:%S")
        .map_err(serde::de::Error::custom)
}

fn map_or_error<'de, D>(deserializer: D) -> Result<Message, D::Error> where D: Deserializer<'de>
{
    let message = Value::deserialize(deserializer)?;
    match message {
        Value::String(error_message) => Ok(Message{error_message, value_map: HashMap::new()}),
        Value::Object(map) => {
            let value_map = map.into_iter().map(|(k, v)| (k, v.as_f64().unwrap()))
                .collect();
            Ok(Message{error_message: "".to_string(), value_map})
        },
        _ => Err(serde::de::Error::custom("unexpected object type"))
    }
}

pub fn build_messages(logger: &Logger, decrypted: String, sensors_map: &HashMap<String, Sensor>)
                  -> Result<Vec<MessageWithDate>, Error> {
    let messages: Vec<ResponseMessage> = serde_json::from_str(&decrypted)?;
    let mut result = Vec::new();
    for message in messages {
        if message.message.error_message.len() > 0 {
            logger.error(message.message.error_message);
        } else {
            let date = message.message_time.year() * 10000 +
                message.message_time.month() as i32 * 100 +
                message.message_time.day() as i32;
            let time = (message.message_time.hour() * 10000 +
                message.message_time.minute() * 100 +
                message.message_time.second()) as i32;
            let sensor = sensors_map.get(&message.sensor_name)
                .ok_or(Error::new(ErrorKind::Other, format!("Unknown sensor {}", message.sensor_name)))?;
            let messages_with_date: Vec<MessageWithDate> = message.message.value_map.into_iter()
                .map(|(k, v)|MessageWithDate {date, time, sensor_id: sensor.id,
                    value_type: k, value: (v * 100.0) as i32 })
                .collect();
            result.extend(messages_with_date);
        }
    }
    Ok(result)
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use std::io::Error;
    use smart_home_common::logger::Logger;
    use crate::messages::{build_messages, Sensor};

    #[test]
    fn test_build_messages() -> Result<(), Error> {
        let logger = Logger::new("test".to_string());
        let sensors_map = HashMap::from([
            ("test_ee".to_string(), Sensor{id: 1, last_timestamp: 1})
        ]);
        let mut messages = build_messages(&logger,
                                          "[{\"messageTime\": \"2020-01-02 15:04:59\", \"sensorName\": \"test_ee\", \"message\": {\"temp\": 0.1}}]".to_string(),
                                          &sensors_map
        )?;
        assert_eq!(messages.len(), 1);
        let message = messages.remove(0);
        assert_eq!(message.sensor_id, 1);
        assert_eq!(message.value_type, "temp");
        assert_eq!(message.value, 10);
        assert_eq!(message.date, 20200102);
        assert_eq!(message.time, 150459);
        Ok(())
    }

    #[test]
    fn test_build_messages2() -> Result<(), Error> {
        let logger = Logger::new("test".to_string());
        let sensors_map = HashMap::from([
            ("zal_int".to_string(), Sensor{id: 1, last_timestamp: 1}),
            ("zal_ext".to_string(), Sensor{id: 2, last_timestamp: 2}),
            ("pump".to_string(),    Sensor{id: 3, last_timestamp: 3})
        ]);
        let messages = build_messages(&logger,
                                          "[{\"messageTime\": \"2021-09-21 19:35:01\", \"sensorName\": \"zal_int\", \"message\": {\"humi\":63.70,\"temp\":23.70}},{\"messageTime\": \"2021-09-21 19:35:01\", \"sensorName\": \"zal_ext\", \"message\": {\"humi\":82.90,\"temp\":11.60}},{\"messageTime\": \"2021-09-21 19:38:57\", \"sensorName\": \"pump\", \"message\": {\"icc\":0.0,\"pres\":2.9}},{\"messageTime\": \"2021-09-21 19:39:08\", \"sensorName\": \"pump\", \"message\": {\"icc\":0.0,\"pres\":2.8}}]".to_string(),
                                          &sensors_map
        )?;
        assert_eq!(messages.len(), 8);
        Ok(())
    }

    #[test]
    fn test_build_messages_error() -> Result<(), Error> {
        let logger = Logger::new("test".to_string());
        let sensors_map = HashMap::from([
            ("test_ee".to_string(), Sensor{id: 1, last_timestamp: 1})
        ]);
        let messages = build_messages(&logger,
                                          "[{\"messageTime\": \"2020-01-02 15:04:45\", \"sensorName\": \"test_ee\", \"message\": \"error\"}]".to_string(),
                                          &sensors_map
        )?;
        assert_eq!(messages.len(), 0);
        Ok(())
    }
}
