use std::env;
use std::io::{Error, ErrorKind};
use postgres::{Client, NoTls};
use crate::entities::location::convert_locations;
use crate::entities::sensor::convert_sensors;
use crate::entities::sensor_data::convert_sensor_data;

mod entities;

fn get_database_connection(db_name: &String) -> Result<Client, Error> {
    Client::connect(format!("postgresql://postgres@localhost/{}", db_name).as_str(), NoTls)
        .map_err(|e| Error::new(ErrorKind::Other, e))
}

fn main() -> Result<(), Error> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 4 {
        println!("Usage: converter source_folder db_name entity_name [params]");
        return Ok(());
    }
    match args[3].as_str() {
        "locations" => convert_locations(get_database_connection(&args[2])?, &args[1]),
        "sensors" => convert_sensors(get_database_connection(&args[2])?, &args[1]),
        "sensor_data" => convert_sensor_data(get_database_connection(&args[2])?, &args[1], args.get(4)),
        _ => { println!("unknown entity name"); Ok(()) }
    }
}
