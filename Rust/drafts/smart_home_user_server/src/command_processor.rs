use std::io::{Error, ErrorKind};
use std::ops::Add;
use chrono::{DateTime, Datelike, Days, Local, Months, TimeDelta, TimeZone, Timelike};
use postgres::{Client, Row};
use smart_home_common::db::DB;

const MAX_UNAGGREGATED_DATA_DAYS: i64 = 7;
const MIN_DATE: i32 = 20190101;
const MAX_TIME: i32 = 235959;

pub struct CommandProcessor {
    db: DB
}

struct SensorDataQuery {
    max_points: usize,
    data_type: String,
    date_or_offset: i32,
    time_or_unit: i32,
    period: i32,
    period_unit: i32
}

impl CommandProcessor {
    pub fn new(db: DB) -> CommandProcessor {
        CommandProcessor { db }
    }

    pub fn execute(&self, command: Vec<u8>) -> Result<Vec<u8>, Error> {
        self.get_sensor_data(build_sensor_data_query(command)?)
    }

    fn get_sensor_data(&self, mut query: SensorDataQuery) -> Result<Vec<u8>, Error> {
        if query.max_points == 0 {
            query.max_points = 1000000;
        }

        let now = Local::now();

        let start_datetime =
            parse_datetime(now, query.date_or_offset, query.time_or_unit)?;
        let mut end_datetime = None;
        if query.period != 0 {
            end_datetime =
                Some(parse_period(start_datetime, query.period, query.period_unit)?);
        }

        let aggregated = (end_datetime.unwrap_or(now) - start_datetime).num_days() > MAX_UNAGGREGATED_DATA_DAYS;

        let client = self.db.get_database_connection()?;
        let rows = if aggregated
            { run_aggregated_query(client, start_datetime, end_datetime, query.data_type, query.max_points)? }
            else { run_query(client, start_datetime, end_datetime, query.data_type, query.max_points)? };

        Ok(Vec::new())
    }

    pub fn check_message_length(&self, length: usize) -> bool {
        length == 16
    }
}

fn split_datetime(datetime: DateTime<Local>) -> (i32, i32) {
    (datetime.year() * 10000 + (datetime.month() * 100) as i32 + datetime.day() as i32,
     (datetime.hour() * 10000) as i32 + (datetime.minute() * 100) as i32 + datetime.second() as i32)
}

fn add_end_datetime() -> String {
    " AND (e.date <= $4 OR (s.date == $4 AND e.time <= $5))".to_string()
}

fn run_aggregated_query(mut client: Client, start_datetime: DateTime<Local>, end_datetime: Option<DateTime<Local>>,
                          data_type: String, max_points: usize) -> Result<Vec<Row>, Error> {
    let (start_date, start_time) = split_datetime(start_datetime);
    let sql = "".to_string() + add_end_datetime().as_str();
    client.query(&sql, &[])
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn run_query(mut client: Client, start_datetime: DateTime<Local>, end_datetime: Option<DateTime<Local>>,
             data_type: String, max_points: usize) -> Result<Vec<Row>, Error> {
    let (start_date, start_time) = split_datetime(start_datetime);
    let sql = "".to_string() + add_end_datetime().as_str();
    client.query(&sql, &[])
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn build_sensor_data_query(command: Vec<u8>) -> Result<SensorDataQuery, Error> {
    let mut buffer16 = [0u8; 2];
    buffer16.copy_from_slice(&command[0..2]);
    let max_points = u16::from_le_bytes(buffer16) as usize;
    let data_type = String::from_utf8(command[2..6].to_vec())
        .map_err(|e| Error::new(ErrorKind::InvalidData, e))?;
    let mut buffer32 = [0u8; 4];
    buffer32.copy_from_slice(&command[6..10]);
    let date_or_offset = i32::from_le_bytes(buffer32);
    buffer32.copy_from_slice(&command[10..14]);
    let time_or_unit = i32::from_le_bytes(buffer32);
    let period = command[14] as i32;
    let period_unit = command[15] as i32;
    Ok(SensorDataQuery{max_points, data_type, date_or_offset, time_or_unit, period, period_unit})
}

fn parse_datetime(date_now: DateTime<Local>, date: i32, time: i32) -> Result<DateTime<Local>, Error> {
    if date >= 0 {
        if date < MIN_DATE || time < 0 || time > MAX_TIME {
            return Err(Error::new(ErrorKind::InvalidData, "Invalid date or time"));
        }
        Ok(build_datetime(date, time))
    } else {
        parse_period(date_now, -date, time)
    }
}

fn build_datetime(date: i32, time: i32) -> DateTime<Local> {
    Local.with_ymd_and_hms(date / 10000,
                           ((date / 100) % 100) as u32,
                           (date % 100) as u32,
                           (time / 10000) as u32,
                           ((time / 100) % 100) as u32,
                           (time % 100) as u32).unwrap()
}

fn parse_period(start_datetime: DateTime<Local>, period: i32, period_unit: i32)
                -> Result<DateTime<Local>, Error> {
    if period <= 0 {
        return Err(Error::new(ErrorKind::InvalidData, "Invalid period"));
    }
    match period_unit {
        0 => Ok(start_datetime.add(TimeDelta::hours(period as i64))),
        1 => Ok(start_datetime + Days::new(period as u64)),
        2 => Ok(start_datetime + Months::new(period as u32)),
        3 => Ok(start_datetime + Months::new((period * 12) as u32)),
        _ => Err(Error::new(ErrorKind::InvalidData, "Invalid period_unit"))
    }
}
