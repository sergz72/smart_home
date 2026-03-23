use std::io::{Error, ErrorKind};
use aes_gcm::aead::{Aead, OsRng};
use aes_gcm::aead::rand_core::RngCore;
use aes_gcm::Aes128Gcm;

pub fn aes_decrypt(cipher: &Aes128Gcm, encrypted: Vec<u8>, time: u64) -> Result<Vec<u8>, Error> {
    if encrypted.len() < 13 {
        return Err(Error::new(ErrorKind::InvalidData, "data length must be > 12"));
    }
    let mut nonce = [0u8; 12];
    nonce.copy_from_slice(&encrypted[0..12]);
    cipher.decrypt(&nonce.into(), &encrypted[12..])
        .map_err(|e| Error::new(ErrorKind::InvalidData, e.to_string()))
}

pub fn aes_encrypt(cipher: &Aes128Gcm, data: Vec<u8>, time: u64) -> Result<Vec<u8>, Error> {
    let nonce = build_nonce(time);
    let data = cipher.encrypt(&nonce.into(), data.as_slice())
        .map_err(|e| Error::new(ErrorKind::InvalidData, e.to_string()))?;
    let mut result = Vec::from(nonce);
    result.extend_from_slice(&data);
    Ok(result)
}

fn build_nonce(time: u64) -> [u8; 12] {
    let mut nonce = [0u8; 12];
    OsRng.fill_bytes(&mut nonce);
    let time_bytes = time.to_le_bytes();
    nonce[6..].copy_from_slice(&time_bytes[0..6]);
    nonce
}
