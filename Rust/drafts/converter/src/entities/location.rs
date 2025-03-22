use std::fs::File;
use std::io::{BufReader, Error, ErrorKind};
use std::path::{Path, PathBuf};
use postgres::Client;
use serde::Deserialize;

#[derive(Debug, Deserialize, Clone)]
#[allow(dead_code)]
struct Location {
    #[serde(rename = "Id")]
    pub id: usize,
    #[serde(rename = "Name")]
    pub name: String,
    #[serde(rename = "LocationType")]
    pub location_type: String,
}

fn read_locations_from_json(file_name: PathBuf) -> Result<Vec<Location>, Error> {
    let file = File::open(file_name)?;
    let reader = BufReader::new(file);
    let locations: Vec<Location> = serde_json::from_reader(reader)?;
    Ok(locations)
}

pub fn convert_locations(mut client: Client, folder_name: &String) -> Result<(), Error> {
    let locations = read_locations_from_json(Path::new(folder_name).join("locations.json"))?;
    for location in locations {
        let id = location.id as i16;
        client.execute("insert into locations(id, name, location_type) values($1, $2, $3)",
        &[&id, &location.name, &location.location_type])
            .map_err(|e|Error::new(ErrorKind::Other, e))?;
    }
    Ok(())
}