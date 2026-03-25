mod configuration;
mod message_processor;

use std::env::args;
use std::io::Error;
use std::thread;
use smart_home_common::base_server::BaseServer;
use smart_home_common::db::create_database_from_configuration;
use crate::configuration::load_configuration;
use crate::message_processor::build_message_processor;

fn main() -> Result<(), Error> {
    let mut arguments = args();
    let ini_file_name = &arguments.nth(1).expect("no file name given");
    let dry_run = arguments.nth(2) == Some("--dry-run".to_string());
    if dry_run {
        println!("Dry run mode");
    }
    let config = load_configuration(ini_file_name)?;
    let database = create_database_from_configuration(&config.database_configuration)?;
    let device_sensors = database.build_device_sensors()?;
    let message_processor = 
        build_message_processor(&config.device_key_file_name, device_sensors, database, dry_run)?;
    if config.tcp_port_number != 0 {
        let pn = config.tcp_port_number;
        let processor = message_processor.clone();
        let tcp_server = Box::leak(Box::new(
            BaseServer::new(false, pn, processor, "tcp_server".to_string())?));
        thread::spawn(move ||tcp_server.start());
    }
    let udp_server =
        Box::leak(Box::new(BaseServer::new(true, config.port_number,
                                           message_processor.clone(),
                                           "udp_server".to_string())?));
    udp_server.start();
    Ok(())
}
