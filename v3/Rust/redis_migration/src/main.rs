use std::collections::HashMap;
use std::env;
use std::io::{Error, ErrorKind};
use std::time::SystemTime;
use chrono::{Datelike, DurationRound, MappedLocalTime, TimeDelta, TimeZone, Timelike, Utc};
use chrono_tz::Tz;
use postgres::NoTls;

fn main() -> Result<(), Error> {
    let mut postgres_host_name = "127.0.0.1".to_string();
    let mut postgres_db_name = "".to_string();
    let mut redis_host_name = "127.0.0.1".to_string();
    let mut timezone = "".to_string();
    let mut create_time_series = false;
    let mut create_aggregations = false;
    let mut create_yearly_aggregations = false;
    let mut run_yearly_aggregations_from = None;
    let mut run_daily_aggregations_from = None;
    let mut load_new_data = false;
    let mut delete_rules = false;
    let mut dry_run = false;
    let args: Vec<String> = env::args().collect();
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--postgres_host_name" => postgres_host_name = get_next_argument(&args, &mut i)?,
            "--postgres_db_name" => postgres_db_name = get_next_argument(&args, &mut i)?,
            "--redis_host_name" => redis_host_name = get_next_argument(&args, &mut i)?,
            "--timezone" => timezone = get_next_argument(&args, &mut i)?,
            "--create_time_series" => create_time_series = true,
            "--create_aggregations" => create_aggregations = true,
            "--run_daily_aggregations_from" => run_daily_aggregations_from =
                Some(get_next_argument(&args, &mut i)?.parse::<i32>()
                    .map_err(|e| Error::new(ErrorKind::InvalidInput, e))?),
            "--create_yearly_aggregations" => create_yearly_aggregations = true,
            "--delete_rules" => delete_rules = true,
            "--run_yearly_aggregations_from" => run_yearly_aggregations_from =
                Some(get_next_argument(&args, &mut i)?.parse::<i32>()
                    .map_err(|e| Error::new(ErrorKind::InvalidInput, e))?),
            "--load_new_data" => load_new_data = true,
            "--dry_run" => dry_run = true,
            _ => return Err(Error::new(ErrorKind::InvalidInput, format!("Unknown argument: {}", args[i]))),
        }
        i += 1;
    }
    if timezone.is_empty() {
        usage();
        return Err(Error::new(ErrorKind::InvalidInput, "Missing timezone"));
    }
    let tz: Tz = timezone.parse()
        .map_err(|e| Error::new(ErrorKind::InvalidInput, e))?;
    if !create_time_series && !load_new_data && !create_aggregations  && !create_yearly_aggregations
        && run_yearly_aggregations_from.is_none() && !delete_rules && run_daily_aggregations_from.is_none() {
        usage();
        return Err(Error::new(ErrorKind::InvalidInput,
                              "At least one of --create_time_series or --create_aggregations or --create_yearly_aggregations or --load_new_data or --run_yearly_aggregations_from should be provided"));
    }
    if dry_run {
        println!("Dry run mode");
    }
    let mut postgres_client = if create_time_series | load_new_data {
        if postgres_db_name.is_empty() {
            usage();
            return Err(Error::new(ErrorKind::InvalidInput, "Missing postgres_db_name"));
        }
        let c = Some(postgres::Client::connect(
            &format!("postgresql://postgres@{}/{}", postgres_host_name, postgres_db_name), NoTls)
            .map_err(|e| Error::new(ErrorKind::Other, e))?);
        println!("Connected to postgres...");
        c
    } else {None};
    let redis_client = redis::Client::open(format!("redis://{}", redis_host_name))
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let mut redis_connection = redis_client.get_connection()
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    println!("Connected to redis...");
    if create_time_series {
        do_create_time_series(postgres_client.as_mut().unwrap(), &mut redis_connection)?;
    }
    if create_aggregations {
        do_create_aggregations(&mut redis_connection)?;
    }
    if create_yearly_aggregations {
        do_create_yearly_aggregations(&mut redis_connection)?;
    }
    if load_new_data {
        do_load_new_data(tz, postgres_client.unwrap(), &mut redis_connection)?;
    }
    if let Some(year) = run_yearly_aggregations_from {
        run_yearly_aggregations(&tz, &mut redis_connection, year, dry_run)?;
    }
    if delete_rules {
        do_delete_rules(&mut redis_connection)?;
    }
    if let Some(date) = run_daily_aggregations_from {
        run_daily_aggregations(&tz, &mut redis_connection, date, dry_run)?;
    }
    Ok(())
}

fn usage() {
    println!("Usage: redis_migration --timezone value [--postgres_host_name value] [--postgres_db_name value] [--redis_host_name value] [--create_time_series|--create_aggregations|--create_yearly_aggregations|--run_yearly_aggregations_from year|--load_new_data]");
}

fn do_load_new_data(tz: Tz, mut postgres_client: postgres::Client, redis_connection: &mut redis::Connection)
                    -> Result<(), Error> {
    let now = SystemTime::now();
    let results: Vec<(String, Vec<(String, String)>, Vec<(u64, f64)>)> = redis::cmd("TS.MGET")
        .arg("FILTER")
        .arg("type=sensor")
        .query(redis_connection)
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

    let (event_date, event_time) = from_millis(&tz, max_timestamp as i64);
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
        let timestamp_opt = to_millis(&tz, event_date, event_time);
        if let Some(timestamp) = timestamp_opt {
            let _: () = redis::cmd("TS.ADD")
                .arg(format!("{}:{}", sensor_id, value_type))
                .arg(timestamp)
                .arg(value as f64 / 100.0)
                .query(redis_connection)
                .map_err(|e| Error::new(ErrorKind::Other, e))?;
            counter += 1;
        }
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds, {} rows added", elapsed, counter);
    Ok(())
}

fn from_millis(tz: &Tz, timestamp: i64) -> (i32, i32) {
    let datetime = tz.timestamp_millis_opt(timestamp).unwrap();
    let date = datetime.year() * 10000 + (datetime.month() * 100) as i32 + datetime.day() as i32;
    let time = (datetime.hour() * 10000 + datetime.minute() * 100 + datetime.second()) as i32;
    (date, time)
}

fn to_millis(tz: &Tz, event_date: i32, event_time: i32) -> Option<i64> {
    match tz.with_ymd_and_hms(
        event_date / 10000,
        ((event_date / 100) % 100) as u32,
        (event_date % 100) as u32,
        (event_time / 10000) as u32,
        ((event_time / 100) % 100) as u32,
        (event_time % 100) as u32) {
        MappedLocalTime::Single(dt) => Some(dt.timestamp_millis()),
        MappedLocalTime::Ambiguous(_, latest) => Some(latest.timestamp_millis()),
        MappedLocalTime::None => None
    }
}

fn ts_create(redis_connection: &mut redis::Connection, name: &String, typ: &str) -> Result<(), Error> {
    let _: () = redis::cmd("TS.CREATE")
        .arg(name)
        .arg("ENCODING")
        .arg("COMPRESSED")
        .arg("DUPLICATE_POLICY")
        .arg("LAST")
        .arg("LABELS")
        .arg("type")
        .arg(typ)
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    Ok(())
}

fn ts_createrule(redis_connection: &mut redis::Connection, value: &String, parent: &String, agg_name: &str) -> Result<(), Error> {
    let _: () = redis::cmd("TS.CREATERULE")
        .arg(value)
        .arg(parent)
        .arg("AGGREGATION")
        .arg(agg_name)
        .arg(86400000)
        .arg(0)
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    Ok(())
}

fn ts_deleterule(redis_connection: &mut redis::Connection, value: &String, parent: &String) -> Result<(), Error> {
    let _: () = redis::cmd("TS.DELETERULE")
        .arg(value)
        .arg(parent)
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    Ok(())
}

fn ts_del(redis_connection: &mut redis::Connection, key: &String, from: i64) -> Result<(), Error> {
    let _: () = redis::cmd("TS.DEL")
        .arg(key)
        .arg(from)
        .arg("+")
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    Ok(())
}

fn ts_range(redis_connection: &mut redis::Connection, key: &String, from: i64) -> Result<Vec<(i64, f64)>, Error> {
    redis::cmd("TS.RANGE")
        .arg(key)
        .arg(from)
        .arg("+")
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn ts_add(redis_connection: &mut redis::Connection, key: &String, value: (i64, f64)) -> Result<(), Error> {
    let _: () = redis::cmd("TS.ADD")
        .arg(key)
        .arg(value.0)
        .arg(value.1)
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    Ok(())
}

fn do_create_aggregations(redis_connection: &mut redis::Connection) -> Result<(), Error> {
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

        ts_create(redis_connection, &min_agg_name, "aggregation")?;
        ts_create(redis_connection, &avg_agg_name, "aggregation")?;
        ts_create(redis_connection, &max_agg_name, "aggregation")?;

        ts_createrule(redis_connection, &value, &min_agg_name, "min")?;
        ts_createrule(redis_connection, &value, &avg_agg_name, "avg")?;
        ts_createrule(redis_connection, &value, &max_agg_name, "max")?;
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds", elapsed);
    Ok(())
}

fn do_delete_rules(redis_connection: &mut redis::Connection) -> Result<(), Error> {
    let now = SystemTime::now();
    let results: Vec<String> = redis::cmd("TS.QUERYINDEX")
        .arg("type=sensor")
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    for value in results {
        println!("Delete rules for sensor {}", value);
        let min_agg_name = format!("{}:min", value);
        let avg_agg_name = format!("{}:avg", value);
        let max_agg_name = format!("{}:max", value);

        ts_deleterule(redis_connection, &value, &min_agg_name)?;
        ts_deleterule(redis_connection, &value, &avg_agg_name)?;
        ts_deleterule(redis_connection, &value, &max_agg_name)?;
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds", elapsed);
    Ok(())
}

fn do_create_yearly_aggregations(redis_connection: &mut redis::Connection) -> Result<(), Error> {
    let now = SystemTime::now();
    let results: Vec<String> = redis::cmd("TS.QUERYINDEX")
        .arg("type=sensor")
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    for value in results {
        println!("Creating yearly aggregations for sensor {}", value);
        let min_agg_name = format!("{}:min_y", value);
        let avg_agg_name = format!("{}:avg_y", value);
        let max_agg_name = format!("{}:max_y", value);

        ts_create(redis_connection, &min_agg_name, "aggregation_y")?;
        ts_create(redis_connection, &avg_agg_name, "aggregation_y")?;
        ts_create(redis_connection, &max_agg_name, "aggregation_y")?;
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds", elapsed);
    Ok(())
}

fn run_daily_aggregations(tz: &Tz, redis_connection: &mut redis::Connection, from: i32, dry_run: bool) -> Result<(), Error> {
    let now = SystemTime::now();
    let results: Vec<String> = redis::cmd("TS.QUERYINDEX")
        .arg("type=sensor")
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let from_timestamp = if from > 0 {
        to_millis(tz, from, 0).unwrap()
    } else {
        let now = Utc::now();
        let date = tz.with_ymd_and_hms(now.year(), now.month(), now.day(), 0, 0, 0).unwrap();
        (date - TimeDelta::days(-from as i64)).timestamp_millis()
    };
    println!("from_timestamp={}", from_timestamp);
    for value in results {
        println!("Building daily aggregations for sensor {}", value);
        let min_agg_name = format!("{}:min", value);
        let avg_agg_name = format!("{}:avg", value);
        let max_agg_name = format!("{}:max", value);

        if !dry_run {
            ts_del(redis_connection, &min_agg_name, from_timestamp)?;
            ts_del(redis_connection, &avg_agg_name, from_timestamp)?;
            ts_del(redis_connection, &max_agg_name, from_timestamp)?;
        }

        let data = ts_range(redis_connection, &value, from_timestamp)?;
        let grouped = group_by(data, |timestamp|build_day_start_timestamp(tz, timestamp));
        let mut min_agg = grouped.iter().map(|(_, value)|get_min(value))
            .collect::<Vec<(i64, f64)>>();
        min_agg.sort_by(|a, b| a.0.cmp(&b.0));
        let mut avg_agg = grouped.iter().map(|(_, value)|get_avg(value))
            .collect::<Vec<(i64, f64)>>();
        avg_agg.sort_by(|a, b| a.0.cmp(&b.0));
        let mut max_agg = grouped.iter().map(|(_, value)|get_max(value))
            .collect::<Vec<(i64, f64)>>();
        max_agg.sort_by(|a, b| a.0.cmp(&b.0));

        for v in min_agg.into_iter() {
            if dry_run {
                println!("{}:{},{}", min_agg_name, v.0, v.1);
            } else {
                ts_add(redis_connection, &min_agg_name, v)?;
            }
        }
        for v in avg_agg.into_iter() {
            if dry_run {
                println!("{}:{},{}", avg_agg_name, v.0, v.1);
            } else {
                ts_add(redis_connection, &avg_agg_name, v)?;
            }
        }
        for v in max_agg.into_iter() {
            if dry_run {
                println!("{}:{},{}", max_agg_name, v.0, v.1);
            } else {
                ts_add(redis_connection, &max_agg_name, v)?;
            }
        }
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds", elapsed);
    Ok(())
}

fn get_min(data: &Vec<(i64, f64)>) -> (i64, f64) {
    let (t, v) = data.iter().min_by(|a, b| a.1.total_cmp(&b.1)).unwrap();
    (*t, *v)
}

fn get_max(data: &Vec<(i64, f64)>) -> (i64, f64) {
    let (t, v) = data.iter().max_by(|a, b| a.1.total_cmp(&b.1)).unwrap();
    (*t, *v)
}

fn get_avg(data: &Vec<(i64, f64)>) -> (i64, f64) {
    let sum: f64 = data.iter().map(|v|v.1).sum();
    (data[data.len()/2].0, sum/data.len() as f64)
}

fn build_day_start_timestamp(tz: &Tz, timestamp: i64) -> i64 {
    let datetime = tz.timestamp_millis_opt(timestamp).unwrap();
    datetime.duration_trunc(TimeDelta::days(1)).unwrap().timestamp_millis()
}

fn build_year_start_timestamp(tz: &Tz, timestamp: i64) -> i64 {
    let datetime = tz.timestamp_millis_opt(timestamp).unwrap();
    tz.with_ymd_and_hms(datetime.year(), 1, 1, 0, 0, 0).unwrap().timestamp_millis()
}

fn group_by<F>(data: Vec<(i64, f64)>, group_builder: F) -> HashMap<i64, Vec<(i64, f64)>>
    where F: Fn(i64) -> i64 {
    let mut result = HashMap::new();
    for (timestamp, value) in data {
        let group_key = group_builder(timestamp);
        result.entry(group_key).or_insert_with(Vec::new).push((timestamp, value));
    }
    result
}

fn run_yearly_aggregations(tz: &Tz, redis_connection: &mut redis::Connection, mut from_year: i32, dry_run: bool) -> Result<(), Error> {
    let now = SystemTime::now();
    let results: Vec<String> = redis::cmd("TS.QUERYINDEX")
        .arg("type=sensor")
        .query(redis_connection)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    if from_year < 0 {
        from_year = (Utc::now() - TimeDelta::days(-from_year as i64)).year();
    }
    let from_timestamp = to_millis(tz, from_year * 10000 + 0101, 0).unwrap();
    println!("from_timestamp={}", from_timestamp);
    for value in results {
        println!("Building yearly aggregations for sensor {}", value);
        let min_agg_name = format!("{}:min", value);
        let avg_agg_name = format!("{}:avg", value);
        let max_agg_name = format!("{}:max", value);
        let min_yagg_name = format!("{}:min_y", value);
        let avg_yagg_name = format!("{}:avg_y", value);
        let max_yagg_name = format!("{}:max_y", value);

        run_yearly_aggregation(tz, redis_connection, min_agg_name, min_yagg_name, from_timestamp, get_min, dry_run)?;
        run_yearly_aggregation(tz, redis_connection, avg_agg_name, avg_yagg_name, from_timestamp, get_avg, dry_run)?;
        run_yearly_aggregation(tz, redis_connection, max_agg_name, max_yagg_name, from_timestamp, get_max, dry_run)?;
    }
    let elapsed = now.elapsed().unwrap().as_millis();
    println!("Redis queries: elapsed time {} milliseconds", elapsed);
    Ok(())
}

fn run_yearly_aggregation(tz: &Tz, redis_connection: &mut redis::Connection, key: String, yearly_key: String,
                          from_timestamp: i64, f: fn(&Vec<(i64,f64)>) -> (i64,f64), dry_run: bool) -> Result<(), Error> {
    let data = ts_range(redis_connection, &key, from_timestamp)?;

    if !dry_run {
        ts_del(redis_connection, &yearly_key, from_timestamp)?;
    }

    let grouped = group_by(data, |timestamp|build_year_start_timestamp(tz, timestamp));
    let mut agg = grouped.iter().map(|(_, value)|f(value))
        .collect::<Vec<(i64, f64)>>();

    agg.sort_by(|a, b| a.0.cmp(&b.0));

    for v in agg.into_iter() {
        if dry_run {
            println!("{}:{},{}", yearly_key, v.0, v.1);
        } else {
            ts_add(redis_connection, &yearly_key, v)?;
        }
    }

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
        ts_create(redis_connection, &format!("{}:{}", sensor_id, value_type), "sensor")?;
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

#[cfg(test)]
mod tests {
    use chrono_tz::Tz;
    use crate::{from_millis, to_millis};

    #[test]
    fn test_timestamp_convert() {
        let tz: Tz = "Europe/Berlin".parse().unwrap();
        let millis = to_millis(&tz, 20260321, 90000);
        assert_eq!(millis, Some(1774080000000));
        let (date, time) = from_millis(&tz, millis.unwrap());
        assert_eq!(date, 20260321);
        assert_eq!(time, 90000);
    }

    #[test]
    fn test_timestamp_convert_2() {
        let tz: Tz = "Europe/Berlin".parse().unwrap();
        let millis = to_millis(&tz, 20221030, 20039);
        assert_eq!(millis, Some(1667091639000));
        let (date, time) = from_millis(&tz, millis.unwrap());
        assert_eq!(date, 20221030);
        assert_eq!(time, 20039);

        let millis = to_millis(&tz, 20230326, 20112);
        assert_eq!(millis, None);
    }
}