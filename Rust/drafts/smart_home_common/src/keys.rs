use std::fs;
use std::fs::File;
use std::io::{BufReader, Error, ErrorKind, Read};

pub fn read_key_file32(key_file_name: &String) -> Result<[u8; 32], Error> {
    let file_size = fs::metadata(key_file_name)?.len();
    if file_size != 32 {
        return Err(Error::new(ErrorKind::InvalidData, "invalid file length"));
    }
    let f = File::open(key_file_name)?;
    let mut reader = BufReader::new(f);
    let mut buffer = [0; 32];
    reader.read(&mut buffer)?;
    Ok(buffer)
}
