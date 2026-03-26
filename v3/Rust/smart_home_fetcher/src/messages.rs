use std::collections::HashMap;
use std::fmt::{Display, Formatter};
use std::io::{Error, ErrorKind};
use chrono::NaiveDateTime;
use serde::{Deserialize, Deserializer};
use serde_json::Value;
use smart_home_common::entities::{Message, MessageDateTime, Messages, SensorTimestamp};
use smart_home_common::logger::Logger;

#[allow(dead_code)]
struct InputMessage {
    error_message: String,
    value_map: HashMap<String, f64>
}

impl Display for InputMessage {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        if self.error_message.len() != 0 {
            return write!(f, "Error message: {}", self.error_message);
        }
        write!(f, "ValueMap: {:?}", self.value_map)
    }
}

#[derive(Deserialize)]
#[allow(dead_code)]
struct ResponseMessage {
    #[serde(deserialize_with = "datefmt", rename = "messageTime")]
    message_time: NaiveDateTime,
    #[serde(rename = "sensorName")]
    sensor_name: String,
    #[serde(deserialize_with = "map_or_error")]
    message: InputMessage
}

impl Display for ResponseMessage {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "MessageTime: {}, SensorName: {}, Message: {}", self.message_time, self.sensor_name, self.message)
    }
}

fn datefmt<'de, D>(deserializer: D) -> Result<NaiveDateTime, D::Error> where D: Deserializer<'de>
{
    let time = String::deserialize(deserializer)?;
    NaiveDateTime::parse_from_str(&time, "%Y-%m-%d %H:%M:%S")
        .map_err(serde::de::Error::custom)
}

fn map_or_error<'de, D>(deserializer: D) -> Result<InputMessage, D::Error> where D: Deserializer<'de>
{
    let message = Value::deserialize(deserializer)?;
    match message {
        Value::String(error_message) => Ok(InputMessage{error_message, value_map: HashMap::new()}),
        Value::Object(map) => {
            let value_map = map.into_iter().map(|(k, v)| (k, v.as_f64().unwrap()))
                .collect();
            Ok(InputMessage{error_message: "".to_string(), value_map})
        },
        _ => Err(serde::de::Error::custom("unexpected object type"))
    }
}

pub fn build_messages(logger: &Logger, decrypted: String,
                      sensors_map: &HashMap<String, SensorTimestamp>, dry_run: bool)
                  -> Result<Vec<Messages>, Error> {
    let messages: Vec<ResponseMessage> = serde_json::from_str(&decrypted)?;
    let mut result = Vec::new();
    for message in messages {
        if message.message.error_message.len() > 0 {
            logger.error(message.message.error_message);
        } else {
            if dry_run {
                logger.info(format!("Message: {}", message));
            }
            let sensor = sensors_map.get(&message.sensor_name)
                .ok_or(Error::new(ErrorKind::Other, format!("Unknown sensor {}", message.sensor_name)))?;
            let messages_with_date: Vec<Message> = message.message.value_map.into_iter()
                .map(|(k, v)|Message {value_type: k, value: (v * 100.0) as i32 })
                .collect();
            result.push(Messages{messages: messages_with_date, sensor_id: sensor.id,
                date_time: MessageDateTime{date_time: message.message_time, timestamp: None}});
        }
    }
    Ok(result)
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use std::io::Error;
    use smart_home_common::logger::Logger;
    use crate::messages::{build_messages, SensorTimestamp};

    #[test]
    fn test_build_messages() -> Result<(), Error> {
        let logger = Logger::new("test".to_string());
        let sensors_map = HashMap::from([
            ("test_ee".to_string(), SensorTimestamp{id: 1, last_timestamp: 1})
        ]);
        let mut messages_vec = build_messages(&logger,
                                          "[{\"messageTime\": \"2020-01-02 15:04:59\", \"sensorName\": \"test_ee\", \"message\": {\"temp\": 0.1}}]".to_string(),
                                          &sensors_map
        )?;
        assert_eq!(messages_vec.len(), 1);
        let mut messages = messages_vec.remove(0);
        let (date, time) = messages.date_time.to_date_and_time();
        assert_eq!(date, 20200102);
        assert_eq!(time, 150459);
        assert_eq!(messages.messages.len(), 1);
        assert_eq!(messages.sensor_id, 1);
        let message = messages.messages.remove(0);
        assert_eq!(message.value_type, "temp");
        assert_eq!(message.value, 10);
        Ok(())
    }

    #[test]
    fn test_build_messages2() -> Result<(), Error> {
        let logger = Logger::new("test".to_string());
        let sensors_map = HashMap::from([
            ("zal_int".to_string(), SensorTimestamp{id: 1, last_timestamp: 1}),
            ("zal_ext".to_string(), SensorTimestamp{id: 2, last_timestamp: 2}),
            ("pump".to_string(),    SensorTimestamp{id: 3, last_timestamp: 3})
        ]);
        let messages = build_messages(&logger,
"[\
{\"messageTime\": \"2021-09-21 19:35:01\", \"sensorName\": \"zal_int\", \"message\": {\"humi\":63.70,\"temp\":23.70}},\
{\"messageTime\": \"2021-09-21 19:35:01\", \"sensorName\": \"zal_ext\", \"message\": {\"humi\":82.90,\"temp\":11.60}},\
{\"messageTime\": \"2021-09-21 19:38:57\", \"sensorName\": \"pump\", \"message\": {\"icc\":0.0,\"pres\":2.9}},\
{\"messageTime\": \"2021-09-21 19:39:08\", \"sensorName\": \"pump\", \"message\": {\"icc\":0.0,\"pres\":2.8}}\
]".to_string(), &sensors_map
        )?;
        assert_eq!(messages.len(), 4);
        assert_eq!(messages[0].messages.len(), 2);
        assert_eq!(messages[0].sensor_id, 1);
        assert_eq!(messages[1].messages.len(), 2);
        assert_eq!(messages[1].sensor_id, 2);
        assert_eq!(messages[2].messages.len(), 2);
        assert_eq!(messages[2].sensor_id, 3);
        assert_eq!(messages[3].messages.len(), 2);
        assert_eq!(messages[3].sensor_id, 3);
        Ok(())
    }

    #[test]
    fn test_build_messages_error() -> Result<(), Error> {
        let logger = Logger::new("test".to_string());
        let sensors_map = HashMap::from([
            ("test_ee".to_string(), SensorTimestamp{id: 1, last_timestamp: 1})
        ]);
        let messages = build_messages(&logger,
                                          "[{\"messageTime\": \"2020-01-02 15:04:45\", \"sensorName\": \"test_ee\", \"message\": \"error\"}]".to_string(),
                                          &sensors_map
        )?;
        assert_eq!(messages.len(), 0);
        Ok(())
    }
}
