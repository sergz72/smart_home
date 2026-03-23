use std::env;
use std::io::{Error, ErrorKind};
use chrono::{Datelike, Local, TimeZone};
use postgres::{Client, NoTls};

fn get_database_connection(db_name: &String) -> Result<Client, Error> {
    Client::connect(format!("postgresql://postgres@localhost/{}", db_name).as_str(), NoTls)
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn main() -> Result<(), Error> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        println!("Usage: db_refresh host_name db_name [date]");
        return Ok(());
    }
    let host_name = &args[1];
    let db_name = &args[2];
    let mut client = get_database_connection(db_name)?;
    let start_from = if args.len() == 4 {
        args[3].parse::<i32>().unwrap()
    } else {
        let row = client.query_one("select max(event_date) from sensor_events", &[])
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let max_event_date: i32 = row.get(0);
        println!("Max event date: {}", max_event_date);
        let date_start_from = Local.with_ymd_and_hms(max_event_date / 10000,
                                                     ((max_event_date / 100) % 100) as u32,
                                                     (max_event_date % 100) as u32,
                                                     0, 0, 0).unwrap() + chrono::Duration::days(-1);
        date_start_from.year() * 10000 + (date_start_from.month() * 100) as i32 +
            date_start_from.day() as i32
    };
    println!("Start from: {}", start_from);
    process_table(&mut client, db_name, host_name, "sensor_events", start_from,
                  "id, sensor_id, event_date, event_time, value_type, value",
                    "id int, sensor_id smallint, event_date int, event_time int, value_type char(4), value int")?;
    process_table(&mut client, db_name, host_name, "sensor_events_aggregated", start_from,
                  "id, event_date, sensor_id, value_type, min_value, max_value, avg_value",
                  "id int, event_date int, sensor_id smallint, value_type char(4), min_value int, max_value int, avg_value int")?;
    Ok(())
}

fn process_table(client: &mut Client, db_name: &String, host_name: &String, table_name: &str, start_from: i32, fields: &str,
                 definitions: &str) -> Result<(), Error> {
    let mut sql = format!("delete from {} where event_date >= $1", table_name);
    println!("{}", sql);
    let mut rows = client.execute(&sql, &[&start_from])
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    println!("Deleted {} rows from {}", rows, table_name);
    sql = format!("insert into {} select * from dblink('dbname={} host={} user=postgres',
                     'select {} from {} where event_date >= {}') as t({})",
                  table_name, db_name, host_name, fields, table_name, start_from, definitions);
    println!("{}", sql);
    rows = client.execute(&sql, &[])
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    println!("Inserted {} rows to {}", rows, table_name);
    Ok(())
}
