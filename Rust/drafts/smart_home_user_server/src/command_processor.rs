use std::collections::HashMap;
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

struct Aggregated {
    min: i32,
    avg: i32,
    max: i32
}

struct SensorDataValues {
    time: i32,
    values: HashMap<String, i32>
}

struct SensorData {
    date: i32,
    values: Option<SensorDataValues>,
    aggregated: Option<HashMap<String, Aggregated>>
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
            { run_aggregated_query(client, start_datetime, end_datetime, query.data_type)? }
            else { run_query(client, start_datetime, end_datetime, query.data_type)? };
        let by_sensor_id: HashMap<i16, Vec<SensorData>> = aggregate_by_sensor_id(rows, aggregated)
            .into_iter()
            .map(|(k,v)|(k, aggregate_by_max_points(v, query.max_points)))
            .collect();
        Ok(Vec::new())
    }

    pub fn check_message_length(&self, length: usize) -> bool {
        length == 16
    }
}

fn aggregate_by_max_points(data: Vec<SensorData>, max_points: usize) -> Vec<SensorData> {
    todo!()
}

fn aggregate_by_sensor_id(rows: Vec<Row>, aggregated: bool) -> HashMap<i16, Vec<SensorData>> {
    let mut result = HashMap::new();
    for row in rows {
        let id: i16 = row.get(0);
        let data = to_sensor_data(row, aggregated);
        let row = result.entry(id).or_insert(Vec::new());
        row.push(data);
    }
    result
}

fn to_sensor_data(row: Row, aggregated: bool) -> SensorData {
    let event_date: i32 = row.get(1);
    if aggregated {
        let values_string: String = row.get(2);
        let values = parse_aggregated_values(values_string);
        SensorData{date: event_date, values: None, aggregated: Some(values)}
    } else {
        let event_time: i32 = row.get(2);
        let values_string: String = row.get(3);
        let values = parse_values(values_string);
        SensorData{date: event_date, values: Some(SensorDataValues{time: event_time, values}),
                    aggregated: None}
    }
}

fn parse_values(values_string: String) -> HashMap<String, i32> {
    todo!()
}

fn parse_aggregated_values(values_string: String) -> HashMap<String, Aggregated> {
    todo!()
}

fn split_datetime(datetime: DateTime<Local>) -> (i32, i32) {
    (datetime.year() * 10000 + (datetime.month() * 100) as i32 + datetime.day() as i32,
     (datetime.hour() * 10000) as i32 + (datetime.minute() * 100) as i32 + datetime.second() as i32)
}

fn end_datetime_string(add: bool) -> String {
    " and (event_date < $4 OR (event_date = $4 AND event_time <= $5))".to_string()
}

fn end_date_string(add: bool) -> String {
    " and event_date <= $3".to_string()
}

fn run_aggregated_query(mut client: Client, start_datetime: DateTime<Local>, end_datetime: Option<DateTime<Local>>,
                          data_type: String) -> Result<Vec<Row>, Error> {
    let (start_date, _start_time) = split_datetime(start_datetime);
    let sql = "with aggregated as (
    select sensor_id, event_date, value_type,  min(value)::int min_value, avg(value)::int avg_value,
           max(value)::int max_value
      from sensor_events
     where event_date >= $1
       and sensor_id in (select id from sensors where data_type = $2)".to_string() +
        end_date_string(end_datetime.is_some()).as_str() +
"    group by sensor_id, event_date, value_type
)
select sensor_id, event_date,
       json_agg(json_build_object('value_type', value_type, 'min', min_value, 'avg', avg_value, 'max', max_value))
  from aggregated
group by sensor_id, event_date
order by event_date";
    if let Some(end_datetime_value) = end_datetime {
        let (end_date, _end_time) = split_datetime(end_datetime_value);
        client.query(&sql, &[&start_date, &data_type, &end_date])
            .map_err(|e| Error::new(ErrorKind::Other, e))
    } else {
        client.query(&sql, &[&start_date, &data_type])
            .map_err(|e| Error::new(ErrorKind::Other, e))
    }
}

fn run_query(mut client: Client, start_datetime: DateTime<Local>, end_datetime: Option<DateTime<Local>>,
             data_type: String) -> Result<Vec<Row>, Error> {
    let (start_date, start_time) = split_datetime(start_datetime);
    let sql = "select sensor_id, event_date, event_time, json_agg(json_build_object('value_type', value_type, 'value', value)) values
  from sensor_events
 where ((event_date > $1) or (event_date = $1 and event_time >= $2))
   and sensor_id in (select id from sensors where data_type = $3)".to_string() +
        end_datetime_string(end_datetime.is_some()).as_str() +
        " group by sensor_id, event_date, event_time
order by event_date, event_time";
    if let Some(end_datetime_value) = end_datetime {
        let (end_date, end_time) = split_datetime(end_datetime_value);
        client.query(&sql, &[&start_date, &start_time, &data_type, &end_date, &end_time])
            .map_err(|e| Error::new(ErrorKind::Other, e))
    } else {
        client.query(&sql, &[&start_date, &start_time, &data_type])
            .map_err(|e| Error::new(ErrorKind::Other, e))
    }
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
