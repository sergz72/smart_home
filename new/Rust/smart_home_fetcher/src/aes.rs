use std::io::Error;
use aes::Aes128;

pub fn aes_decrypt(cipher: &Aes128, data: Vec<u8>, time: u64) -> Result<String, Error> {
    todo!()
}

pub fn aes_encrypt(cipher: &Aes128, data: Vec<u8>, time: u64) -> Result<Vec<u8>, Error> {
    todo!()
}
