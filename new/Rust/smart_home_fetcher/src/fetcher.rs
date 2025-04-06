use std::collections::HashMap;
use std::io::{Error, ErrorKind, Read};
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use aes_gcm::{Aes128Gcm, KeyInit};
use postgres::Client;
use smart_home_common::db::{insert_messages_with_date_to_db, MessageWithDate, DB};
use smart_home_common::keys::read_key_file16;
use smart_home_common::logger::Logger;
use crate::aes::{aes_decrypt, aes_encrypt};
use crate::configuration::Configuration;
use crate::messages::{build_messages, Sensor};
use crate::network::udp_send;
use flate2::read::GzDecoder;

pub struct Fetcher {
    config: Configuration,
    client: Client,
    sensors_map: HashMap<String, Sensor>,
    aes: Aes128Gcm,
    time: u64,
    timeout: Duration
}

pub fn build_fetcher(config: Configuration) -> Result<Fetcher, Error> {
    let db = DB::new(config.connection_string.clone());
    let mut client = db.get_database_connection()?;
    let sensors_map = build_sensors_map(&mut client)?;
    let key = read_key_file16(&config.key_file_name)?;
    let aes = Aes128Gcm::new(&key.into());
    let time = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis() as u64;
    let timeout = Duration::from_secs(config.timeout as u64);
    Ok(Fetcher { config, client, sensors_map, aes, time, timeout })
}

fn build_sensors_map(client: &mut Client) -> Result<HashMap<String, Sensor>, Error> {
    let rows = client.query("select * from v_sensors_latest_timestamp", &[])
        .map_err(|e|Error::new(ErrorKind::Other, e))?;
    let result: HashMap<String, Sensor> = rows.iter()
        .map(|row| (row.get(0), Sensor{id: row.get(1), last_timestamp: row.get(2)}))
        .collect();
    Ok(result)
}

impl Fetcher {
    pub fn fetch_messages(&mut self) -> Result<(), Error> {
        for (address, sensors) in &self.config.addresses {
            let logger = Logger::new(address.clone());
            let last_timestamp = sensors.iter()
                .map(|s|self.sensors_map.get(s).map(|sensor|sensor.last_timestamp).unwrap_or(0))
                .max()
                .unwrap_or(0);
            let request = build_request(last_timestamp);
            match self.process_request(&logger, request, address) {
                Ok(messages) => {
                    logger.info(format!("messages count {}", messages.len()));
                    if let Err(e) = insert_messages_with_date_to_db(&mut self.client, messages) {
                        logger.error(e.to_string());
                    }
                },
                Err(e) => logger.error(e.to_string())
            }
        }
        Ok(())
    }

    fn process_request(&self, logger: &Logger, request: Vec<u8>, address: &String)
        -> Result<Vec<MessageWithDate>, Error> {
        logger.info(format!("Request size {}", request.len()));
        let encrypted = aes_encrypt(&self.aes, request, self.time)?;
        logger.info(format!("encrypted size {}", encrypted.len()));
        let response = self.send(logger, encrypted, address)?;
        logger.info(format!("response size {}", response.len()));
        let decrypted = aes_decrypt(&self.aes, response, self.time)?;
        logger.info(format!("decrypted size {}", decrypted.len()));
        let decompressed = decompress(decrypted)?;
        logger.info(format!("decompressed size {}", decompressed.len()));
        build_messages(&logger, decompressed, &self.sensors_map)
    }
    
    fn send(&self, logger: &Logger, request: Vec<u8>, address: &String) -> Result<Vec<u8>, Error> {
        for _i in 0..self.config.retries-1 {
            match udp_send(address, &request, self.timeout) {
                Ok(data) => return Ok(data),
                Err(e) => logger.error(e.to_string())
            }
        }
        udp_send(address, &request, self.timeout)
    }
}

fn decompress(data: Vec<u8>) -> Result<String, Error> {
    let mut decoder = GzDecoder::new(&data[..]);
    let mut decompressed = Vec::new();
    decoder.read_to_end(&mut decompressed)?;
    String::from_utf8(decompressed)
        .map_err(|e| Error::new(ErrorKind::InvalidData, e.to_string()))
}

fn build_request(last_timestamp: i64) -> Vec<u8> {
    let request = "GET /sensor_data/all@".to_string() + last_timestamp.to_string().as_str();
    request.as_bytes().to_vec()
}

#[cfg(test)]
mod tests {
    use crate::fetcher::build_request;

    #[test]
    fn test_build_request() {
        let result = build_request(12345678);
        assert_eq!(result, "GET /sensor_data/all@12345678".as_bytes().to_vec())
    }
}