use std::collections::HashMap;
use std::fs::File;
use std::io::{BufReader, Error, ErrorKind};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
#[allow(dead_code)]
pub struct Configuration {
    #[serde(rename = "KeyFileName")]
    pub key_file_name: String,
    #[serde(rename = "PortNumber")]
    pub port_number: u16,
    #[serde(rename = "TcpPortNumber", default)]
    pub tcp_port_number: u16,
    #[serde(rename = "DeviceKeyFileName")]
    pub device_key_file_name: String,
    #[serde(rename = "TimeOffset", default)]
    pub time_offset: i64,
    #[serde(rename = "TotalCalculation", default)]
    pub total_calculation: HashMap<String, i32>
}

pub fn load_configuration(ini_file_name: &String) -> Result<Configuration, Error> {
    let file = File::open(ini_file_name)?;
    let reader = BufReader::new(file);
    let config: Configuration = serde_json::from_reader(reader)?;

    if config.key_file_name.len() == 0 || config.port_number == 0 ||
        config.device_key_file_name.len() == 0
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
        assert_eq!(config.key_file_name, "key.dat", "incorrect key file name");
        assert_eq!(config.port_number, 59999, "incorrect PortNumber value");
        assert_eq!(config.tcp_port_number, 60002, "incorrect TcpPortNumber value");
        assert_eq!(config.device_key_file_name, "device_key.dat", "incorrect device key file name");
        assert_eq!(config.time_offset, -1, "incorrect time offset");
        assert_eq!(config.total_calculation.len(), 1, "incorrect total calculation length");
        assert_eq!(config.total_calculation.contains_key("pwr"), true, "total calculation pwr key is misssing");
        assert_eq!(*config.total_calculation.get("pwr").unwrap(), 12, "incorrect total calculation pwr key value");
    }
}