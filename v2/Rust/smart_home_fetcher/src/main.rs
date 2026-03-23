use std::io::Error;
use crate::configuration::load_configuration;
use crate::fetcher::build_fetcher;

mod configuration;
mod fetcher;
mod messages;
mod network;
mod aes;

fn main() -> Result<(), Error> {
    let ini_file_name = &std::env::args().nth(1).expect("no file name given");
    let config = load_configuration(ini_file_name)?;
    let mut fetcher = build_fetcher(config)?;
    fetcher.fetch_messages()
}
