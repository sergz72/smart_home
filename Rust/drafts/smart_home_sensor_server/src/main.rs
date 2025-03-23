mod configuration;
mod message_processor;

use std::io::Error;
use std::thread;
use smart_home_common::base_server::BaseServer;
use crate::configuration::load_configuration;
use crate::message_processor::build_message_processor;

fn main() -> Result<(), Error> {
    let ini_file_name = &std::env::args().nth(1).expect("no file name given");
    let config = load_configuration(ini_file_name)?;
    let message_processor = build_message_processor(&config.device_key_file_name);
    if config.tcp_port_number != 0 {
        let fname = config.key_file_name.clone();
        let pn = config.tcp_port_number;
        let processor = message_processor.clone();
        let tcp_server = Box::leak(Box::new(BaseServer::new(false, pn, processor, &fname)?));
        thread::spawn(move ||tcp_server.start());
    }
    let udp_server =
        Box::leak(Box::new(BaseServer::new(true, config.port_number, message_processor.clone(), &config.key_file_name)?));
    udp_server.start();
    Ok(())
}
