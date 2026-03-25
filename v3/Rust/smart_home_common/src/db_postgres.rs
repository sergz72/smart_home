use std::collections::HashMap;
use std::io::{Error, ErrorKind};
use chrono::{DateTime, Datelike, Timelike};
use chrono_tz::Tz;
use postgres::{Client, NoTls, Row};
use crate::db::{get_current_date_time, Database};
use crate::entities::{DeviceSensor, LastSensorData, Location, MessageDateTime, Messages, Sensor, SensorDataQuery, SensorDataResult, SensorTimestamp};
use crate::logger::Logger;

pub struct PostgresDatabase {
    connection_string: String,
    tz: Tz
}

impl PostgresDatabase {
    fn build_client(&self) -> Result<Client, Error> {
        Client::connect(&self.connection_string, NoTls)
            .map_err(|e| Error::new(ErrorKind::Other, e))
    }

    pub fn new(connection_string: String, tz: Tz) -> Result<Box<PostgresDatabase>, Error> {
        Ok(Box::new(PostgresDatabase{connection_string, tz}))
    }
}

impl Database for PostgresDatabase {
    fn insert_messages_to_db(&self, messages: Vec<Messages>, logger: &Logger, dry_run: bool) -> Result<(), Error> {
        let mut client = self.build_client()?;
        for msg in messages {
            for message in msg.messages {
                let (date, time) = msg.date_time.to_date_and_time();
                client.execute(
                    "INSERT INTO sensor_events(sensor_id, event_date, event_time, value_type, value)\
                       VALUES ($1, $2, $3, $4, $5)",
                    &[&msg.sensor_id, &date, &time, &message.value_type,
                        &message.value])
                    .map_err(|e| Error::new(ErrorKind::Other, e))?;
            }
        }
        Ok(())
    }

    fn get_sensors(&self) -> Result<HashMap<usize, Sensor>, Error> {
        todo!()
    }

    fn get_locations(&self) -> Result<HashMap<usize, Location>, Error> {
        todo!()
    }

    fn get_sensor_timestamps(&self) -> Result<HashMap<String, SensorTimestamp>, Error> {
        let mut client = self.build_client()?;
        let rows = client.query("select * from v_sensors_latest_timestamp", &[])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
        let result: HashMap<String, SensorTimestamp> = rows.iter()
            .map(|row| (row.get(0), SensorTimestamp{id: row.get(1), last_timestamp: row.get(2)}))
            .collect();
        Ok(result)
    }

    fn get_current_date_time(&self) -> MessageDateTime {
        get_current_date_time(&self.tz)
    }

    fn build_device_sensors(&self) -> Result<HashMap<usize, HashMap<usize, DeviceSensor>>, Error> {
        let mut client = self.build_client()?;
        let result = client.query("
with s as (
select id, device_id, unnest(device_sensors) dsensors
from sensors
where device_id is not null and enabled = true
), so as (
select id, device_id, unnest(offsets) doffsets
from sensors
where device_id is not null and enabled = true
)
select s.id, s.device_id, (s.dsensors).id idx, (s.dsensors).value_type, coalesce((so.doffsets).offset_value, 0) offset_value
from s left join so
  on so.id = s.id and so.device_id = s.device_id and (s.dsensors).value_type = (so.doffsets).value_type", &[])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
        let mut sensor_map = HashMap::new();
        for row in result {
            let sensor_id: i16 = row.get(0);
            let device_id: i16 = row.get(1);
            let sensor_idx: i16 = row.get(2);
            let value_type: String = row.get(3);
            let offset_value: i32 = row.get(4);
            let map = sensor_map
                .entry(device_id as usize)
                .or_insert(HashMap::new());
            map.insert(sensor_idx as usize,DeviceSensor{sensor_id, value_type, offset_value});
        }
        Ok(sensor_map)
    }

    fn get_last_sensor_data(&self) -> Result<LastSensorData, Error> {
        todo!()
    }

    fn get_sensor_data(&self, query: SensorDataQuery) -> Result<SensorDataResult, Error> {
        /*let client = self.build_client()?;
        let rows = if query_parameters.aggregated
        { run_aggregated_query(client, query_parameters)? }
        else { run_query(client, query_parameters)? };
        let by_sensor_id: HashMap<i16, Vec<SensorDataOut>> = aggregate_by_sensor_id(rows, aggregated)
            .into_iter()
            .map(|(k,v)|(k, aggregate_by_max_points(v, query.max_points, aggregated)))
            .collect();
        Ok((by_sensor_id, aggregated))*/
        todo!()
    }

    fn get_time_zone(&self) -> String {
        self.tz.name().to_string()
    }
}

fn get_tomorrow() -> i32 {
    /*let now = Local::now();
    let (date, _time) = split_datetime(now.add(Days::new(1)));
    date*/
    todo!()
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

/*fn aggregate_by_sensor_id(rows: Vec<Row>, aggregated: bool) -> HashMap<i16, Vec<SensorData>> {
    let mut result = HashMap::new();
    for row in rows {
        let id: i16 = row.get(0);
        let data = to_sensor_data(row, aggregated);
        let row = result.entry(id).or_insert(Vec::new());
        row.push(data);
    }
    result
}

fn parse_values(values_json: serde_json::Value) -> HashMap<String, i32> {
    let mut result = HashMap::new();
    for element in values_json.as_array().unwrap() {
        let map = element.as_object().unwrap();
        result.insert(map["value_type"].as_str().unwrap().to_string(), map["value"].as_i64().unwrap() as i32);
    }
    result
}*/

fn split_datetime(datetime: DateTime<Tz>) -> (i32, i32) {
    (datetime.year() * 10000 + (datetime.month() * 100) as i32 + datetime.day() as i32,
     (datetime.hour() * 10000) as i32 + (datetime.minute() * 100) as i32 + datetime.second() as i32)
}

fn run_aggregated_query(mut client: Client, start_datetime: DateTime<Tz>, end_datetime: DateTime<Tz>,
                        data_type: String) -> Result<Vec<Row>, Error> {
    let (start_date, _start_time) = split_datetime(start_datetime);
    let sql = "select sensor_id, event_date,
       json_agg(json_build_object('value_type', value_type, 'min', min_value, 'avg', avg_value, 'max', max_value))
  from sensor_events_aggregated
 where event_date >= $1 and event_date <= $3
   and sensor_id in (select id from sensors where data_type = $2)
group by sensor_id, event_date
order by event_date";
    let (end_date, _end_time) = split_datetime(end_datetime);
    client.query(sql, &[&start_date, &data_type, &end_date])
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn run_query(mut client: Client, start_datetime: DateTime<Tz>, end_datetime: DateTime<Tz>,
             data_type: String) -> Result<Vec<Row>, Error> {
    let (start_date, start_time) = split_datetime(start_datetime);
    let sql = "select sensor_id, event_date, event_time, json_agg(json_build_object('value_type', value_type, 'value', value)) values
  from sensor_events
 where ((event_date > $1) or (event_date = $1 and event_time >= $2))
   and (event_date < $4 OR (event_date = $4 AND event_time <= $5))
   and sensor_id in (select id from sensors where data_type = $3)
group by sensor_id, event_date, event_time
order by event_date, event_time";
    let (end_date, end_time) = split_datetime(end_datetime);
    client.query(sql, &[&start_date, &start_time, &data_type, &end_date, &end_time])
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

#[cfg(test)]
mod tests {
    use std::io::Error;
    use crate::db::{create_database_from_configuration, DatabaseConfiguration};

    #[test]
    fn test_load_device_sensors() -> Result<(), Error> {
        let db = create_database_from_configuration(&DatabaseConfiguration{
            connection_string: "postgresql://postgres@localhost/smart_home".to_string(),
            sensors_file: None,
            locations_file: None,
            time_zone: "Europe/Berlin".to_string(),
        })?;
        let sensors = db.build_device_sensors()?;
        assert_eq!(sensors.len(), 5);
        assert_eq!(sensors.contains_key(&1), true);
        let sensors1 = sensors.get(&1).unwrap();
        assert_eq!(sensors1.len(), 7);
        assert_eq!(sensors1.contains_key(&4), true);
        let sensors14 = sensors1.get(&4).unwrap();
        assert_eq!(sensors14.sensor_id, 6);
        assert_eq!(sensors14.value_type, "humi");
        Ok(())
    }
}
