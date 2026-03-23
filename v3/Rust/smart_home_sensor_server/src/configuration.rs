use std::fs::File;
use std::io::{BufReader, Error, ErrorKind};
use serde::Deserialize;
use smart_home_common::db::DatabaseConfiguration;

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct Configuration {
    #[serde(rename = "DatabaseConfiguration")]
    pub database_configuration: DatabaseConfiguration,
    #[serde(rename = "PortNumber")]
    pub port_number: u16,
    #[serde(rename = "TcpPortNumber", default)]
    pub tcp_port_number: u16,
    #[serde(rename = "DeviceKeyFileName")]
    pub device_key_file_name: String
}

pub fn load_configuration(ini_file_name: &String) -> Result<Configuration, Error> {
    let file = File::open(ini_file_name)?;
    let reader = BufReader::new(file);
    let config: Configuration = serde_json::from_reader(reader)?;
    config.database_configuration.validate()?;

    if config.port_number == 0 || config.device_key_file_name.len() == 0
    {
        return Err(Error::new(
            ErrorKind::InvalidData,
            "incorrect configuration parameters",
        ));
    }

    Ok(config)
}

#[cfg(test)]
mod tests {
    use crate::configuration::load_configuration;

    #[test]
    fn test_load_configuration() {
        let result = load_configuration(&"test_resources/testConfiguration.json".to_string());
        assert!(!result.is_err(), "Configuration load error {}", result.unwrap_err());
        let config = result.unwrap();
        assert_eq!(config.database_configuration.connection_string, "postgresql://postgres@localhost/smart_home", "incorrect connection string");
        assert_eq!(config.database_configuration.time_zone, "Europe/Berlin", "incorrect time zone");
        assert_eq!(config.database_configuration.sensors_file, Some("sensors.json".to_string()), "incorrect sensors file name");
        assert_eq!(config.database_configuration.locations_file, Some("locations.json".to_string()), "incorrect sensors file name");
        assert_eq!(config.port_number, 59999, "incorrect PortNumber value");
        assert_eq!(config.tcp_port_number, 60002, "incorrect TcpPortNumber value");
        assert_eq!(config.device_key_file_name, "device_key.dat", "incorrect device key file name");
    }
}