mod configuration;
mod message_processor;
mod command_processor;
mod sensor_data;

use std::io::Error;
use smart_home_common::base_server::BaseServer;
use smart_home_common::db::DB;
use smart_home_common::keys::read_key_file32;
use crate::configuration::load_configuration;
use crate::message_processor::build_message_processor;

#[derive(Debug)]
pub struct DeviceSensor {
    pub sensor_id: i16,
    pub value_type: String
}

fn main() -> Result<(), Error> {
    let ini_file_name = &std::env::args().nth(1).expect("no file name given");
    let config = load_configuration(ini_file_name)?;
    let db = DB::new(config.connection_string.clone());
    let key = read_key_file32(&config.key_file_name)?;
    let message_processor =
        build_message_processor(key, db)?;
    let udp_server =
        Box::leak(Box::new(BaseServer::new(true, config.port_number,
                                           message_processor.clone(), 0,
                                           "udp_server".to_string())?));
    udp_server.start();
    Ok(())
}

#[cfg(test)]
mod tests {

    #[test]
    fn test_() {
    }
}
