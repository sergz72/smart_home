use std::collections::HashMap;
use std::fs::File;
use std::io::{BufReader, Error, ErrorKind};
use serde::Deserialize;
use smart_home_common::db::DatabaseConfiguration;

#[derive(Debug, Deserialize)]
pub struct Configuration {
    #[serde(rename = "DatabaseConfiguration")]
    pub database_configuration: DatabaseConfiguration,
    #[serde(rename = "KeyFileName")]
    pub key_file_name: String,
    #[serde(rename = "Timeout")]
    pub timeout: usize,
    #[serde(rename = "Retries")]
    pub retries: usize,
    #[serde(rename = "Addresses")]
    pub addresses: HashMap<String, Vec<String>>
}

pub fn load_configuration(ini_file_name: &String) -> Result<Configuration, Error> {
    let file = File::open(ini_file_name)?;
    let reader = BufReader::new(file);
    let config: Configuration = serde_json::from_reader(reader)?;
    config.database_configuration.validate()?;

    if config.timeout == 0 || config.retries == 0
        || config.key_file_name.len() == 0
        || config.addresses.len() == 0 {
        return Err(Error::new(
            ErrorKind::InvalidData,
            "incorrect configuration parameters"
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
        assert_eq!(config.key_file_name, "fetchKey.dat", "incorrect key file name");
        assert_eq!(config.timeout, 10, "incorrect timeout value");
        assert_eq!(config.retries, 3, "incorrect retries value");
        assert_eq!(config.addresses.len(), 3, "incorrect addresses length");
    }

    #[test]
    fn test_load_configuration2() {
        let result = load_configuration(&"test_resources/testConfiguration2.json".to_string());
        assert!(!result.is_err(), "Configuration load error {}", result.unwrap_err());
        let config = result.unwrap();
        assert_eq!(config.database_configuration.connection_string, "postgresql://postgres@localhost/smart_home", "incorrect connection string");
        assert_eq!(config.database_configuration.time_zone, "Europe/Berlin", "incorrect time zone");
        assert_eq!(config.database_configuration.sensors_file, None, "incorrect sensors file name");
        assert_eq!(config.database_configuration.locations_file, None, "incorrect sensors file name");
        assert_eq!(config.key_file_name, "fetchKey.dat", "incorrect key file name");
        assert_eq!(config.timeout, 10, "incorrect timeout value");
        assert_eq!(config.retries, 3, "incorrect retries value");
        assert_eq!(config.addresses.len(), 3, "incorrect addresses length");
    }
}
