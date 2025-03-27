use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use smart_home_common::db::DB;
use smart_home_common::logger::Logger;

const MAX_UNAGGREGATED_DATA_DAYS: i32 = 7;

pub struct CommandProcessor {
    db: DB
}

impl CommandProcessor {
    pub fn new(db: DB) -> CommandProcessor {
        CommandProcessor { db }
    }

    pub fn execute(&self, logger: &Logger, command: Vec<u8>) -> Result<Vec<u8>, Error> {
        let command_string = String::from_utf8(command)
            .map_err(|e| Error::new(ErrorKind::InvalidData, e))?;
        logger.info(format!("Command: {}", command_string));
        if !command_string.starts_with("GET ") {
            return Err(Error::new(ErrorKind::InvalidData, "Invalid command"));
        }
        if command_string[4..].starts_with("/sensor_data?") {
            return self.get_sensor_data(&command_string[17..]);
        }
        Err(Error::new(ErrorKind::InvalidData, "Invalid GET operation"))
    }

    fn get_sensor_data(&self, query: &str) -> Result<Vec<u8>, Error> {
        let parsed = parse_query(query);

        let mut max_points = 1000000;
        if let Some(max_points_string) = parsed.get("max_points") {
            max_points = max_points_string.parse()
                .map_err(|e|Error::new(ErrorKind::InvalidInput, "Invalid max_points value"))?;
        }

        let data_type = parsed.get("data_type")
            .ok_or(Error::new(ErrorKind::InvalidInput, "Missing data_type"))?;

        let start_string = parsed.get("start")
            .ok_or(Error::new(ErrorKind::InvalidInput, "Missing start"))?;
        let (start_date, start_time) = parse_start(start_string)?;

        let mut end_date = None;
        let mut end_time = None;
        if let Some(period_string) = parsed.get("period") {
            let (end_date_value, end_time_value) =
                parse_period(period_string, start_date, start_time)?;
            end_date = Some(end_date_value);
            end_time = Some(end_time_value);
        }

        let aggregated = days_between(start_date, end_date) > MAX_UNAGGREGATED_DATA_DAYS;

        //let query = aggregated ? build_aggregated_query(start_date, start_time, data_type);

        let mut client = self.db.get_database_connection()?;
        //let rows = client.query("");
        Ok(Vec::new())
    }
}

fn days_between(start_date: i32, end_date: Option<i32>) -> i32 {
    todo!()
}

fn parse_start(start_string: &String) -> Result<(i32, i32), Error> {
    todo!()
}

fn parse_period(period_string: &String, start_date: i32, start_time: i32) -> Result<(i32, i32), Error> {
    todo!()
}

fn parse_query(query: &str) -> HashMap<String, String> {
    query
        .split('&')
        .filter_map(|s| {
            s.split_once('=')
                .and_then(|t| Some((t.0.to_owned(), t.1.to_owned())))
        })
        .collect()
}
