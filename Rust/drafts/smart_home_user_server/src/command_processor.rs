use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use std::ops::Add;
use chrono::{DateTime, Datelike, Days, Local, Months, TimeDelta, TimeZone, Timelike};
use postgres::{Client, Row};
use rand::rngs::OsRng;
use rand::TryRngCore;
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

#[derive(Clone)]
struct Aggregated {
    min: i32,
    avg: i32,
    max: i32
}

impl Aggregated {
    fn new() -> Aggregated {
        Aggregated{min: i32::MAX, avg: 0, max: i32::MIN}
    }
    
    fn random() -> Aggregated {
        let v = (OsRng.try_next_u32().unwrap() / 10) as i32;
        Aggregated{min: v, avg: v * 2, max: v * 4}
    }
    
    fn add(&mut self, value: &Aggregated) {
        self.avg += value.avg;
        if value.min < self.min {
            self.min = value.min;
        }
        if value.max > self.max {
            self.max = value.max;
        }
    }

    fn calculate_average(&mut self, n: i32) {
        self.avg /= n;
    }
}

#[derive(Clone)]
struct SensorDataValues {
    time: i32,
    values: HashMap<String, i32>
}

impl SensorDataValues {
    fn add(&mut self, values: &HashMap<String, i32>) {
        for (key, value) in values {
            *self.values.entry(key.clone()).or_insert(0) += value;
        }
    }
    
    fn calculate_average(&mut self, n: i32, new_time: i32) {
        self.values = self.values.iter().map(|(k, v)| (k.clone(), *v / n)).collect();
        self.time = new_time;
    }
}

#[derive(Clone)]
struct SensorData {
    date: i32,
    values: Option<SensorDataValues>,
    aggregated: Option<HashMap<String, Aggregated>>
}

struct SensorDataOut {
    date: i32,
    values: Option<Vec<SensorDataValues>>,
    aggregated: Option<HashMap<String, Aggregated>>
}

impl SensorData {
    fn from(source: &SensorData) -> SensorData {
        SensorData{date: source.date, values: source.values.as_ref().map(|v|v.clone()),
                    aggregated: source.aggregated.as_ref().map(|v|v.clone())}
    }

    fn add(&mut self, data: &SensorData) {
        if let Some(values) = &mut self.values {
            values.add(&data.values.as_ref().unwrap().values);
        }
        if let Some(aggregated) = &mut self.aggregated {
            for (key, value) in data.aggregated.as_ref().unwrap() {
                aggregated.entry(key.clone()).or_insert(Aggregated::new()).add(value);
            }
        }
    }
    
    fn calculate_average(&mut self, n: i32, new_time: i32, new_date: i32) {
        if let Some(values) = &mut self.values {
            values.calculate_average(n, new_time);
        }
        if let Some(aggregated) = &mut self.aggregated {
            self.date = new_date;
            for (_key, value) in aggregated {
                value.calculate_average(n);
            }
        }
    }
}

impl SensorDataOut {
    fn to_binary(&self) -> Vec<u8> {
        let mut result = Vec::new();
        result.extend_from_slice(&self.date.to_le_bytes());
        if let Some(values) = &self.values {
            result.extend_from_slice(&(values.len() as u32).to_le_bytes());
            for value in values {
                result.push(value.values.len() as u8);
                result.extend_from_slice(&value.time.to_le_bytes());
                for (key, value) in &value.values {
                    append_key(key, &mut result);
                    result.extend_from_slice(&value.to_le_bytes());
                }
            }
        }
        if let Some(aggregated) = &self.aggregated {
            result.push(aggregated.len() as u8);
            for (key, value) in aggregated {
                append_key(key, &mut result);
                result.extend_from_slice(&value.min.to_le_bytes());
                result.extend_from_slice(&value.avg.to_le_bytes());
                result.extend_from_slice(&value.max.to_le_bytes());
            }
        }
        result
    }
}

fn append_key(key: &String, result: &mut Vec<u8>) {
    let bytes = key.as_bytes();
    result.push(bytes[0]);
    result.push(bytes[1]);
    result.push(bytes[2]);
    result.push(if bytes.len() > 3 { bytes[3] } else { 0x20 });
}

impl CommandProcessor {
    pub fn new(db: DB) -> CommandProcessor {
        CommandProcessor { db }
    }

    pub fn execute(&self, command: Vec<u8>) -> Result<Vec<u8>, Error> {
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
        let by_sensor_id: HashMap<i16, Vec<SensorDataOut>> = aggregate_by_sensor_id(rows, aggregated)
            .into_iter()
            .map(|(k,v)|(k, aggregate_by_max_points(v, query.max_points, aggregated)))
            .collect();
        Ok((by_sensor_id, aggregated))
    }

    pub fn check_message_length(&self, length: usize) -> bool {
        length == 16
    }
}

fn aggregate_by_max_points(data: Vec<SensorData>, max_points: usize, aggregated: bool)
    -> Vec<SensorDataOut> {
    let factor = data.len() / max_points + 1;
    let mut idx = 0;
    let mut result = Vec::new();
    while idx < data.len() {
        let start = idx;
        let mut sd = SensorData::from(data.get(idx).unwrap());
        idx += 1;
        for _i in 1..factor {
            if idx >= data.len() {
                break;
            }
            sd.add(data.get(idx).unwrap());
            idx += 1;
        }
        let n = idx - start;
        let d = data.get(start + n / 2).unwrap();
        sd.calculate_average(n as i32, 
                             d.values.as_ref().map(|v|v.time).unwrap_or(0), d.date);
        result.push(sd);
    }
    to_sensor_data_out(result, aggregated)
}

fn to_sensor_data_out(data: Vec<SensorData>, aggregated: bool) -> Vec<SensorDataOut> {
    let mut map = HashMap::new();
    for d in data {
        let day_data = map.entry(d.date).or_insert(Vec::new());
        day_data.push(d);
    }
    let mut v: Vec<SensorDataOut> = map.into_iter().map(|(k, v)|SensorDataOut{
        date: k,
        aggregated: if aggregated { v[0].aggregated.clone() } else { None },
        values: if aggregated { None } else { Some(aggregate_values(v)) },
    }).collect();
    v.sort_by(|a,b|a.date.cmp(&b.date));
    v
}

fn aggregate_values(values: Vec<SensorData>) -> Vec<SensorDataValues> {
    values.into_iter().map(|v|v.values.unwrap()).collect()
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
        result.insert(map["value_type"].to_string(), map["value"].as_i64().unwrap() as i32);
    }
    result
}

fn parse_aggregated_values(values_json: serde_json::Value) -> HashMap<String, Aggregated> {
    let mut result = HashMap::new();
    for element in values_json.as_array().unwrap() {
        let map = element.as_object().unwrap();
        result.insert(map["value_type"].to_string(), Aggregated{
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
     where event_date >= $1
       and sensor_id in (select id from sensors where data_type = $2)".to_string() +
        if end_datetime.is_some() {" and event_date <= $3" } else { "" } +
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
        if end_datetime.is_some() { " and (event_date < $4 OR (event_date = $4 AND event_time <= $5))" } else { "" } +
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

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use std::io::Error;
    use rand::rngs::OsRng;
    use rand::TryRngCore;
    use smart_home_common::db::DB;
    use crate::command_processor::{aggregate_by_max_points, Aggregated, CommandProcessor, SensorData, SensorDataQuery, SensorDataValues};

    #[test]
    fn test_get_sensor_data() -> Result<(), Error> {
        let processor = CommandProcessor::new(
            DB::new("postgresql://postgres@localhost/smart_home".to_string())
        );
        let (result, aggregated) = processor.get_sensor_data(
            SensorDataQuery{max_points: 200, data_type: "env".to_string(),
                                    date_or_offset: 20250301, time_or_unit: 0, period: 0, period_unit: 0},
        )?;
        assert_eq!(true, aggregated);
        Ok(())
    }
    
    fn generate_values(time: i32, value_types: &[&str]) -> SensorDataValues {
        SensorDataValues{time, 
            values: value_types.into_iter().map(|v|(v.to_string(), OsRng.try_next_u32().unwrap() as i32)).collect()}
    }
    
    #[test]
    fn test_aggregate_by_max_points_raw() {
        let source = vec![
            SensorData{date:20250331, values: Some(generate_values(100000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(100500, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(101000, &["humi", "pres", "temp", "co2"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(101500, &["humi", "pres", "temp", "co2"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(102000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(102500, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(103000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(103500, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(104000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(104500, &["humi", "pres", "temp"])), aggregated: None}
        ];
        let mut result = aggregate_by_max_points(source.clone(), 3, false);
        assert_eq!(1, result.len());
        let data = result.remove(0);
        assert_eq!(data.date, 20250331);
        assert!(data.aggregated.is_none());
        assert!(data.values.is_some());
        let mut values = data.values.unwrap();
        assert_eq!(3, values.len());
        let value = values.remove(0);
        assert_eq!(value.time, 101000);
        check_raw_values(value.values, &source[0..4]);
        let value = values.remove(0);
        assert_eq!(value.time, 103000);
        check_raw_values(value.values, &source[4..8]);
        let value = values.remove(0);
        assert_eq!(value.time, 104500);
        check_raw_values(value.values, &source[8..10]);
    }

    fn check_raw_values(values: HashMap<String, i32>, data: &[SensorData]) {
        let mut average = HashMap::new();
        for value in data {
            for (key, data) in &value.values.as_ref().unwrap().values {
                *average.entry(key.clone()).or_insert(0) += *data;
            }
        }
        average = average.into_iter().map(|(k, v)| (k, v / (data.len() as i32))).collect();
        assert_eq!(average.len(), values.len());
        for (k, v) in average {
            assert_eq!(*values.get(&k).unwrap(), v);
        }
    }

    fn generate_aggregated(value_types: &[&str]) -> HashMap<String, Aggregated> {
        value_types.into_iter().map(|v|(v.to_string(), Aggregated::random())).collect()
    }
    
    #[test]
    fn test_aggregate_by_max_points_aggregated() {
        let source = vec![
            SensorData{date:20250331, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250401, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250402, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp", "co2"]))},
            SensorData{date:20250403, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp", "co2"]))},
            SensorData{date:20250404, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250405, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250406, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250407, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250408, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250409, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
        ];
        let mut result = aggregate_by_max_points(source.clone(), 3, true);
        assert_eq!(3, result.len());
        let data = result.remove(0);
        assert_eq!(data.date, 20250402);
        assert!(data.aggregated.is_some());
        assert!(data.values.is_none());
        check_aggregated_values(data.aggregated.unwrap(), &source[0..4]);
        let data = result.remove(0);
        assert_eq!(data.date, 20250406);
        assert!(data.aggregated.is_some());
        assert!(data.values.is_none());
        check_aggregated_values(data.aggregated.unwrap(), &source[4..8]);
        let data = result.remove(0);
        assert_eq!(data.date, 20250409);
        assert!(data.aggregated.is_some());
        assert!(data.values.is_none());
        check_aggregated_values(data.aggregated.unwrap(), &source[8..10]);
    }
    
    fn check_aggregated_values(values: HashMap<String, Aggregated>, data: &[SensorData]) {
        let mut average = HashMap::new();
        for value in data {
            for (key, data) in value.aggregated.as_ref().unwrap() {
                average.entry(key.clone()).or_insert(Aggregated::new()).add(data);
            }
        }
        for (_key, agg) in &mut average {
            agg.calculate_average(data.len() as i32);
        }
        assert_eq!(average.len(), values.len());
        for (k, a) in average {
            let v = values.get(&k).unwrap();
            assert_eq!(v.min, a.min);
            assert_eq!(v.avg, a.avg);
            assert_eq!(v.max, a.max);
        }
    }
}