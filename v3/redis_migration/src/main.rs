use std::env;
use std::io::{Error, ErrorKind};
use std::time::SystemTime;
use chrono::{Datelike, TimeZone, Timelike, Utc};
use postgres::NoTls;
use redis::Connection;

fn main() -> Result<(), Error> {
    let mut postgres_host_name = "127.0.0.1".to_string();
    let mut postgres_db_name = "".to_string();
    let mut redis_host_name = "127.0.0.1".to_string();
    let mut create_time_series = false;
    let mut create_aggregations = false;
    let mut load_new_data = false;
    let args: Vec<String> = env::args().collect();
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--postgres_host_name" => postgres_host_name = get_next_argument(&args, &mut i)?,
            "--postgres_db_name" => postgres_db_name = get_next_argument(&args, &mut i)?,
            "--redis_host_name" => redis_host_name = get_next_argument(&args, &mut i)?,
            "--create_time_series" => create_time_series = true,
            "--create_aggregations" => create_aggregations = true,
            "--load_new_data" => load_new_data = true,
            _ => return Err(Error::new(ErrorKind::InvalidInput, format!("Unknown argument: {}", args[i]))),
        }
        i += 1;
    }
    if postgres_db_name.is_empty() {
        usage();
        return Err(Error::new(ErrorKind::InvalidInput, "Missing postgres_db_name"));
    }
    if !create_time_series && !load_new_data && !create_aggregations {
        usage();
        return Err(Error::new(ErrorKind::InvalidInput, "At least one of --create_time_series or --create_aggregations or --load_new_data should be provided"));
    }
    let mut postgres_client = postgres::Client::connect(
        &format!("postgresql://postgres@{}/{}", postgres_host_name, postgres_db_name), NoTls)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    println!("Connected to postgres...");
    let redis_client = redis::Client::open(format!("redis://{}", redis_host_name))
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let mut redis_connection = redis_client.get_connection()
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    println!("Connected to redis...");
    if create_time_series {
        do_create_time_series(&mut postgres_client, &mut redis_connection)?;
    }
    if create_aggregations {
        do_create_aggregations(&mut redis_connection)?;
    }
    if load_new_data {
        do_load_new_data(postgres_client, redis_connection)?;
    }
    Ok(())
}

fn usage() {
    println!("Usage: redis_migration [--postgres_host_name value] --postgres_db_name value [--redis_host_name value] [--create_time_series|--load_new_data]");
}

fn do_load_new_data(mut postgres_client: postgres::Client, mut redis_connection: redis::Connection)
                    -> Result<(), Error> {
    let now = SystemTime::now();
    let results: Vec<(String, Vec<(String, String)>, Vec<(u64, f64)>)> = redis::cmd("TS.MGET")
        .arg("FILTER")
        .arg("type=sensor")
        .query(&mut redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let mut max_timestamp = 0;
    for (_, _, values) in results {
        for (timestamp, _) in values {
            if timestamp > max_timestamp {
                max_timestamp = timestamp;
            }
        }
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis MGET query: elapsed time {} milliseconds", elapsed);

    let (event_date, event_time) = from_millis(max_timestamp as i64);
    println!("Loading new data from {}{:06}", event_date, event_time);
    let now = SystemTime::now();
    let rows = postgres_client.query(
        "select sensor_id, event_date, event_time, value_type, value from sensor_events
         where event_date > $1 or (event_date = $1 and event_time > $2) order by event_date, event_time",
        &[&event_date, &event_time])
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Postgres query: elapsed time {} milliseconds", elapsed);

    let now = SystemTime::now();
    let mut counter = 0;
    for row in rows {
        let sensor_id: i16 = row.get(0);
        let event_date: i32 = row.get(1);
        let event_time: i32 = row.get(2);
        let value_type: String = row.get(3);
        let value: i32 = row.get(4);
        let timestamp = to_millis(event_date, event_time);
        let _: () = redis::cmd("TS.ADD")
            .arg(format!("{}:{}", sensor_id, value_type))
            .arg(timestamp)
            .arg(value as f64 / 100.0)
            .query(&mut redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        counter += 1;
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds, {} rows added", elapsed, counter);
    Ok(())
}

fn from_millis(timestamp: i64) -> (i32, i32) {
    let datetime = Utc.timestamp_millis_opt(timestamp).unwrap();
    let date = datetime.year() * 10000 + (datetime.month() * 100) as i32 + datetime.day() as i32;
    let time = (datetime.hour() * 10000 + datetime.minute() * 100 + datetime.second()) as i32;
    (date, time)
}

fn to_millis(event_date: i32, event_time: i32) -> i64 {
    Utc.with_ymd_and_hms(
        event_date / 10000,
        ((event_date / 100) % 100) as u32,
        (event_date % 100) as u32,
        (event_time / 10000) as u32,
        ((event_time / 100) % 100) as u32,
        (event_time % 100) as u32).unwrap().timestamp_millis()
}

fn do_create_aggregations(redis_connection: &mut Connection) -> Result<(), Error> {
    let now = SystemTime::now();
    let results: Vec<String> = redis::cmd("TS.QUERYINDEX")
        .arg("type=sensor")
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    for value in results {
        println!("Creating aggregations for sensor {}", value);
        let min_agg_name = format!("{}:min", value);
        let avg_agg_name = format!("{}:avg", value);
        let max_agg_name = format!("{}:max", value);

        let _: () = redis::cmd("TS.CREATE")
            .arg(&min_agg_name)
            .arg("ENCODING")
            .arg("COMPRESSED")
            .arg("DUPLICATE_POLICY")
            .arg("LAST")
            .arg("LABELS")
            .arg("type")
            .arg("aggregation")
            .query(redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;

        let _: () = redis::cmd("TS.CREATE")
            .arg(&avg_agg_name)
            .arg("ENCODING")
            .arg("COMPRESSED")
            .arg("DUPLICATE_POLICY")
            .arg("LAST")
            .arg("LABELS")
            .arg("type")
            .arg("aggregation")
            .query(redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;

        let _: () = redis::cmd("TS.CREATE")
            .arg(&max_agg_name)
            .arg("ENCODING")
            .arg("COMPRESSED")
            .arg("DUPLICATE_POLICY")
            .arg("LAST")
            .arg("LABELS")
            .arg("type")
            .arg("aggregation")
            .query(redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;

        let _: () = redis::cmd("TS.CREATERULE")
            .arg(&value)
            .arg(&min_agg_name)
            .arg("AGGREGATION")
            .arg("min")
            .arg(86400000)
            .arg(0)
            .query(redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;

        let _: () = redis::cmd("TS.CREATERULE")
            .arg(&value)
            .arg(&avg_agg_name)
            .arg("AGGREGATION")
            .arg("avg")
            .arg(86400000)
            .arg(0)
            .query(redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;

        let _: () = redis::cmd("TS.CREATERULE")
            .arg(&value)
            .arg(&max_agg_name)
            .arg("AGGREGATION")
            .arg("max")
            .arg(86400000)
            .arg(0)
            .query(redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds", elapsed);
    Ok(())
}

fn do_create_time_series(postgres_client: &mut postgres::Client,
                         redis_connection: &mut redis::Connection) -> Result<(), Error> {
    let now = SystemTime::now();
    let rows = postgres_client.query("select distinct sensor_id, value_type from sensor_events", &[])
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let elapsed = now.elapsed().unwrap().as_micros();
    println!("Postgres query: elapsed time {} microseconds", elapsed);
    let now = SystemTime::now();
    for row in rows {
        let sensor_id: i16 = row.get(0);
        let value_type: String = row.get(1);
        let _: () = redis::cmd("TS.CREATE")
            .arg(format!("{}:{}", sensor_id, value_type))
            .arg("ENCODING")
            .arg("COMPRESSED")
            .arg("DUPLICATE_POLICY")
            .arg("LAST")
            .arg("LABELS")
            .arg("type")
            .arg("sensor")
            .query(redis_connection)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
    }
    let elapsed = now.elapsed().unwrap().as_micros();
    println!("Redis queries: elapsed time {} microseconds", elapsed);
    Ok(())
}

fn get_next_argument(args: &Vec<String>, idx: &mut usize) -> Result<String, Error> {
    let next_idx = *idx + 1;
    if next_idx >= args.len() {
        Err(Error::new(ErrorKind::InvalidInput, "Missing argument value"))
    } else {
        *idx = next_idx;
        Ok(args[next_idx].clone())
    }
}
