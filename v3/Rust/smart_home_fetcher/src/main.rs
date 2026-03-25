use std::env::args;
use std::io::Error;
use crate::configuration::load_configuration;
use crate::fetcher::build_fetcher;

mod configuration;
mod fetcher;
mod messages;
mod network;
mod aes;

fn main() -> Result<(), Error> {
    let mut arguments = args();
    let ini_file_name = &arguments.nth(1).expect("no file name given");
    let dry_run = arguments.nth(2) == Some("--dry-run".to_string());
    if dry_run {
        println!("Dry run mode");
    }
    let config = load_configuration(ini_file_name)?;
    let mut fetcher = build_fetcher(config)?;
    fetcher.fetch_messages(dry_run)
}
