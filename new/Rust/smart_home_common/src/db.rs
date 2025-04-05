use std::fmt::{Display, Formatter};
use std::io::{Error, ErrorKind};
use std::ops::Add;
use chrono::{Datelike, Local, Timelike};
use postgres::{Client, NoTls};

pub struct DB {
    connection_string: String
}

pub struct Message {
    pub sensor_id: i16,
    pub value_type: String,
    pub value: i32
}

impl Display for Message {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "message sensor_id={} value_type={} value={}", self.sensor_id, self.value_type, self.value)
    }
}

impl DB {
    pub fn new(connection_string: String) -> DB {
        DB{connection_string}
    }

    pub fn get_database_connection(&self) -> Result<Client, Error> {
        Client::connect(&self.connection_string, NoTls)
            .map_err(|e| Error::new(ErrorKind::Other, e))
    }
    
    pub fn insert_messages_to_db(&self, messages: Vec<Message>, time_offset: i64) -> Result<(), Error> {
        let datetime = Local::now().add(chrono::Duration::hours(time_offset));
        let date = datetime.year() * 10000 + (datetime.month() * 100) as i32 + datetime.day() as i32;
        let time = (datetime.hour() * 10000) as i32 + (datetime.minute() * 100) as i32 + datetime.second() as i32;
        for message in messages {
            self.insert_message(message, date, time)?;
        }
        Ok(())
    }
    
    fn insert_message(&self, message: Message, date: i32, time: i32) -> Result<(), Error> {
        let mut client = self.get_database_connection()?;
        client.execute("INSERT INTO sensor_events(sensor_id, event_date, event_time, value_type, value)\
        VALUES ($1, $2, $3, $4, $5)", &[&message.sensor_id, &date, &time, &message.value_type, &message.value])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use crate::db::DB;

    #[test]
    fn test_insert_messages_to_db() {
        let db = DB::new("postgresql://postgres@localhost/smart_home".to_string());
    }
}
