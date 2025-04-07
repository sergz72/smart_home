use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use std::ops::Add;
use chrono::{DateTime, Datelike, Days, Local, Months, TimeDelta, TimeZone, Timelike};
use postgres::{Client, Row};
use smart_home_common::db::DB;
use smart_home_common::user_message_processor::CommandProcessor;
use crate::sensor_data::{aggregate_by_max_points, Aggregated, SensorData, SensorDataOut, SensorDataValues};

const MAX_UNAGGREGATED_DATA_DAYS: i64 = 7;
const MIN_DATE: i32 = 20190101;

pub struct UserCommandProcessor {
    db: DB,
    time_offset: i64
}

struct SensorDataQuery {
    max_points: usize,
    data_type: String,
    date_or_offset: i32,
    offset_unit: i32,
    period: i32,
    period_unit: i32
}

impl CommandProcessor for UserCommandProcessor {
    fn check_message_length(&self, length: usize) -> bool {
        length == 12 || length == 2
    }

    fn execute(&self, command: Vec<u8>) -> Result<Vec<u8>, Error> {
        match command[0] {
            0 => self.execute_sensors_query(command.len()),
            1 => self.execute_last_data_query(command[1..].to_vec()),
            2 => self.execute_sensor_data_query(command[1..].to_vec()),
            _ => Err(Error::new(ErrorKind::InvalidInput, "Invalid command"))
        }
    }
}

impl UserCommandProcessor {
    pub fn new(db: DB, time_offset: i64) -> Box<UserCommandProcessor> {
        Box::new(UserCommandProcessor { db, time_offset })
    }

    fn execute_sensors_query(&self, command_length: usize) -> Result<Vec<u8>, Error> {
        if command_length != 2 {
            return Err(Error::new(ErrorKind::InvalidInput, "Invalid sensors command length"));
        }
        let rows = self.get_sensors()?;
        Ok(sensor_rows_to_binary(rows))
    }
    
    fn get_sensors(&self) -> Result<Vec<Row>, Error> {
        let mut client = self.db.get_database_connection()?;
        let sql = "select s.id, s.data_type, l.name, l.location_type from sensors s, locations l
where s.location_id = l.id";
        client.query(sql, &[])
            .map_err(|e| Error::new(ErrorKind::Other, e))
    }

    fn execute_last_data_query(&self, command: Vec<u8>) -> Result<Vec<u8>, Error> {
        if command.len() != 1 {
            return Err(Error::new(ErrorKind::InvalidInput, "Invalid last data command length"));
        }
        let now = Local::now() - Days::new(command[0] as u64);
        let (date, _time) = split_datetime(now);
        let data = self.get_last_sensor_data(date)?;
        let mut result = Vec::new();
        result.push(0); // no error
        result.push(data.len() as u8);
        for (sensor_id, sdata) in data {
            result.push(sensor_id as u8);
            result.extend_from_slice(&sdata.to_binary());
        }
        Ok(result)
    }
    
    fn get_last_sensor_data(&self, date: i32) -> Result<HashMap<i16, SensorData>, Error> {
        let client = self.db.get_database_connection()?;
        let rows = run_last_query(client, date)?;
        Ok(rows.into_iter().map(|row|(row.get(0), to_sensor_data(row, false))).collect())
    }

    pub fn execute_sensor_data_query(&self, command: Vec<u8>) -> Result<Vec<u8>, Error> {
        let (data, aggregated) = self.get_sensor_data(build_sensor_data_query(command)?)?;
        let mut result = Vec::new();
        result.push(if aggregated { 1 } else { 0 });
        for (sensor_id, d) in data {
            result.push(sensor_id as u8);
            result.extend_from_slice(&(d.len() as u16).to_le_bytes());
            for sd in d {
                result.extend_from_slice(&sd.to_binary());
            }
        }
        Ok(result)
    }

    fn get_sensor_data(&self, mut query: SensorDataQuery)
        -> Result<(HashMap<i16, Vec<SensorDataOut>>, bool), Error> {
        if query.max_points == 0 {
            query.max_points = 1000000;
        }

        let now = Local::now();

        let start_datetime =
            self.parse_datetime(now, query.date_or_offset, query.offset_unit)?;
        let mut end_datetime = None;
        if query.period != 0 {
            end_datetime =
                Some(self.parse_period(start_datetime, query.period, query.period_unit)?);
        }

        let aggregated = (end_datetime.unwrap_or(now) - start_datetime).num_days() > MAX_UNAGGREGATED_DATA_DAYS;

        let client = self.db.get_database_connection()?;
        let rows = if aggregated
            { run_aggregated_query(client, start_datetime, end_datetime, query.data_type)? }
            else { run_query(client, start_datetime, end_datetime, query.data_type)? };
        let by_sensor_id: HashMap<i16, Vec<SensorDataOut>> = aggregate_by_sensor_id(rows, aggregated)
            .into_iter()
            .map(|(k,v)|(k, aggregate_by_max_points(v, query.max_points, aggregated)))
            .collect();
        Ok((by_sensor_id, aggregated))
    }

    fn parse_period(&self, start_datetime: DateTime<Local>, period: i32, period_unit: i32)
                    -> Result<DateTime<Local>, Error> {
        if period >= 0 {
            match period_unit {
                0 => Ok(start_datetime + Days::new(period as u64)),
                1 => Ok(start_datetime + Months::new(period as u32)),
                2 => Ok(start_datetime + Months::new((period * 12) as u32)),
                _ => Err(Error::new(ErrorKind::InvalidData, "Invalid period_unit"))
            }
        } else {
            match period_unit {
                0 => Ok(start_datetime - TimeDelta::hours((-period * 24) as i64 - self.time_offset)),
                1 => Ok(start_datetime - Months::new(-period as u32)),
                2 => Ok(start_datetime - Months::new((-period * 12) as u32)),
                _ => Err(Error::new(ErrorKind::InvalidData, "Invalid period_unit"))
            }
        }
    }

    fn parse_datetime(&self, date_now: DateTime<Local>, date: i32, unit: i32) -> Result<DateTime<Local>, Error> {
        if date >= 0 {
            if date < MIN_DATE {
                return Err(Error::new(ErrorKind::InvalidData, "Invalid date"));
            }
            Ok(build_datetime(date, 0))
        } else {
            self.parse_period(date_now, date, unit)
        }
    }
}

fn get_tomorrow() -> i32 {
    let now = Local::now();
    let (date, _time) = split_datetime(now.add(TimeDelta::days(1)));
    date
}

fn run_last_query(mut client: Client, date: i32) -> Result<Vec<Row>, Error> {
    let tomorrow = get_tomorrow();
    let sql = "with last_data as (
    select sensor_id, max(event_date::bigint * 1000000 + event_time::bigint) max_datetime
      from sensor_events
     where event_date >= $1 and event_date <= $2
    group by sensor_id
)
select e.sensor_id, e.event_date, e.event_time, json_agg(json_build_object('value_type', value_type, 'value', value)) values
  from sensor_events e, last_data l
 where e.event_date >= $1 and event_date <= $2 and l.sensor_id = e.sensor_id and l.max_datetime / 1000000 = e.event_date and l.max_datetime % 1000000 = e.event_time
group by e.sensor_id, e.event_date, e.event_time";
    client.query(sql, &[&date, &tomorrow])
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn sensor_rows_to_binary(rows: Vec<Row>) -> Vec<u8> {
    let mut result = Vec::new();
    result.push(0); // no error
    result.extend_from_slice(&(rows.len() as u8).to_le_bytes());
    for row in rows {
        let id: i16 = row.get(0);
        let data_type: String = row.get(1);
        let location: String = row.get(2);
        let location_type: String = row.get(3);
        result.push(id as u8);
        let bytes = data_type.as_bytes(); 
        result.push(bytes[0]);
        result.push(bytes[1]);
        result.push(bytes[2]);
        result.push(location.len() as u8);
        result.extend_from_slice(location.as_bytes());
        let bytes = location_type.as_bytes();
        result.push(bytes[0]);
        result.push(bytes[1]);
        result.push(bytes[2]);
    }
    result
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
        let values_json: serde_json::Value = row.get(2);
        let values = parse_aggregated_values(values_json);
        SensorData{date: event_date, values: None, aggregated: Some(values)}
    } else {
        let event_time: i32 = row.get(2);
        let values_json: serde_json::Value = row.get(3);
        let values = parse_values(values_json);
        SensorData{date: event_date, values: Some(SensorDataValues{time: event_time, values}),
                    aggregated: None}
    }
}

fn parse_values(values_json: serde_json::Value) -> HashMap<String, i32> {
    let mut result = HashMap::new();
    for element in values_json.as_array().unwrap() {
        let map = element.as_object().unwrap();
        result.insert(map["value_type"].as_str().unwrap().to_string(), map["value"].as_i64().unwrap() as i32);
    }
    result
}

fn parse_aggregated_values(values_json: serde_json::Value) -> HashMap<String, Aggregated> {
    let mut result = HashMap::new();
    for element in values_json.as_array().unwrap() {
        let map = element.as_object().unwrap();
        result.insert(map["value_type"].as_str().unwrap().to_string(), Aggregated{
            min: map["min"].as_i64().unwrap() as i32,
            avg: map["avg"].as_i64().unwrap() as i32,
            max: map["max"].as_i64().unwrap() as i32
        });
    }
    result
}

fn split_datetime(datetime: DateTime<Local>) -> (i32, i32) {
    (datetime.year() * 10000 + (datetime.month() * 100) as i32 + datetime.day() as i32,
     (datetime.hour() * 10000) as i32 + (datetime.minute() * 100) as i32 + datetime.second() as i32)
}

fn run_aggregated_query(mut client: Client, start_datetime: DateTime<Local>, end_datetime: Option<DateTime<Local>>,
                          data_type: String) -> Result<Vec<Row>, Error> {
    let (start_date, _start_time) = split_datetime(start_datetime);
    let sql = "with aggregated as (
    select sensor_id, event_date, value_type,  min(value)::int min_value, avg(value)::int avg_value,
           max(value)::int max_value
      from sensor_events
     where event_date >= $1 and event_date <= $3
       and sensor_id in (select id from sensors where data_type = $2)
    group by sensor_id, event_date, value_type
)
select sensor_id, event_date,
       json_agg(json_build_object('value_type', value_type, 'min', min_value, 'avg', avg_value, 'max', max_value))
  from aggregated
group by sensor_id, event_date
order by event_date";
    let (end_date, _end_time) = split_datetime(end_datetime.unwrap_or(Local::now().add(TimeDelta::days(1))));
    client.query(sql, &[&start_date, &data_type, &end_date])
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn run_query(mut client: Client, start_datetime: DateTime<Local>, end_datetime: Option<DateTime<Local>>,
             data_type: String) -> Result<Vec<Row>, Error> {
    let (start_date, start_time) = split_datetime(start_datetime);
    let sql = "select sensor_id, event_date, event_time, json_agg(json_build_object('value_type', value_type, 'value', value)) values
  from sensor_events
 where ((event_date > $1) or (event_date = $1 and event_time >= $2))
   and (event_date < $4 OR (event_date = $4 AND event_time <= $5))
   and sensor_id in (select id from sensors where data_type = $3)
group by sensor_id, event_date, event_time
order by event_date, event_time";
    let (end_date, end_time) = split_datetime(end_datetime.unwrap_or(Local::now().add(TimeDelta::days(1))));
    client.query(sql, &[&start_date, &start_time, &data_type, &end_date, &end_time])
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn build_sensor_data_query(command: Vec<u8>) -> Result<SensorDataQuery, Error> {
    let mut buffer16 = [0u8; 2];
    buffer16.copy_from_slice(&command[0..2]);
    let max_points = u16::from_le_bytes(buffer16) as usize;
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
    Ok(SensorDataQuery{max_points, data_type, date_or_offset, offset_unit, period, period_unit})
}

fn build_datetime(date: i32, time: i32) -> DateTime<Local> {
    Local.with_ymd_and_hms(date / 10000,
                           ((date / 100) % 100) as u32,
                           (date % 100) as u32,
                           (time / 10000) as u32,
                           ((time / 100) % 100) as u32,
                           (time % 100) as u32).unwrap()
}

#[cfg(test)]
mod tests {
    use std::io::Error;
    use smart_home_common::db::DB;
    use crate::command_processor::{UserCommandProcessor, SensorDataQuery};

    #[test]
    fn test_get_sensor_data() -> Result<(), Error> {
        let processor = UserCommandProcessor::new(
            DB::new("postgresql://postgres@localhost/smart_home".to_string()), 0
        );
        let (result, aggregated) = processor.get_sensor_data(
            SensorDataQuery{max_points: 200, data_type: "env".to_string(),
                                    date_or_offset: 20250301, offset_unit: 0, period: 0, period_unit: 0},
        )?;
        assert_eq!(true, aggregated);
        Ok(())
    }

    #[test]
    fn test_get_sensors() -> Result<(), Error> {
        let processor = UserCommandProcessor::new(
            DB::new("postgresql://postgres@localhost/smart_home".to_string()), 0
        );
        let result = processor.get_sensors()?;
        assert_eq!(result.len(), 8);
        Ok(())
    }

    #[test]
    fn test_get_last_sensor_data() -> Result<(), Error> {
        let processor = UserCommandProcessor::new(
            DB::new("postgresql://postgres@localhost/smart_home".to_string()), 0
        );
        let result = processor.get_last_sensor_data(20250301)?;
        assert_eq!(result.len(), 4);
        Ok(())
    }
}