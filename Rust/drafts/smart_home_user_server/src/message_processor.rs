use std::io::{Error, ErrorKind, Read};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use bzip2::Compression;
use bzip2::read::BzEncoder;
use chacha20::ChaCha20;
use chacha20::cipher::{KeyIvInit, StreamCipher};
use rand::TryRngCore;
use rand::rngs::OsRng;
use smart_home_common::base_server::MessageProcessor;
use smart_home_common::db::DB;
use smart_home_common::logger::Logger;
use crate::command_processor::CommandProcessor;

const MAX_TIME_DIFFERENCE: u64 = 60;

struct UserMessageProcessor {
    key: [u8; 32],
    command_processor: CommandProcessor
}

impl UserMessageProcessor {
    fn new(key: [u8; 32], db: DB)
        -> Result<UserMessageProcessor, Error> {
        Ok(UserMessageProcessor {key, command_processor: CommandProcessor::new(db)})
    }
}

impl MessageProcessor for UserMessageProcessor {
    fn process_message(&self, logger: &Logger, message: &Vec<u8>, _time_offset: i64) -> Vec<u8> {
        match self.process_message_with_result(logger, message) {
            Ok(result) => result,
            Err(error) => {
                logger.error(format!("process_message error: {}", error));
                Vec::new()
            }
        }
    }
}

impl UserMessageProcessor {
    fn process_message_with_result(&self, logger: &Logger, message: &Vec<u8>)
        -> Result<Vec<u8>, Error> {
        let command = self.decrypt(message)?;
        let response = self.command_processor.execute(logger, command)?;
        self.encrypt_response(response)
    }

    fn transform(&self, iv: &[u8], data: &[u8]) -> Result<Vec<u8>, Error> {
        let mut cipher = ChaCha20::new((&self.key).into(), iv.into());
        let mut out_vec = vec![0u8; data.len()];
        let out_bytes = out_vec.as_mut_slice();
        cipher.apply_keystream_b2b(data, out_bytes)
            .map_err(|e| Error::new(ErrorKind::Other, e.to_string()))?;
        Ok(out_vec)
    }

    fn decrypt(&self, message: &Vec<u8>) -> Result<Vec<u8>, Error> {
        if message.len() < 13 {
            return Err(Error::new(ErrorKind::InvalidData, "message is too short"));
        }
        let iv = &message[0..12];
        check_iv(iv)?;
        let encrypted = &message[12..];
        self.transform(iv, encrypted)
    }

    fn encrypt_response(&self, response: Vec<u8>) -> Result<Vec<u8>, Error> {
        let compressed = compress(response)?;
        self.encrypt(compressed)
    }

    fn encrypt(&self, data: Vec<u8>) -> Result<Vec<u8>, Error> {
        let iv = build_iv()?;
        let bytes = data.as_slice();
        let mut out_vec = self.transform(&iv, bytes)?;
        let mut result = Vec::from(iv);
        result.append(&mut out_vec);
        Ok(result)
    }
}

fn compress(data: Vec<u8>) -> Result<Vec<u8>, Error> {
    let mut compressor = BzEncoder::new(data.as_slice(), Compression::best());
    let mut result = Vec::new();
    compressor.read_to_end(&mut result)?;
    Ok(result)
}

fn check_iv(iv: &[u8]) -> Result<(), Error> {
    let random_part = &iv[0..4];
    let mut time_bytes = [0u8; 8];
    time_bytes.copy_from_slice(&iv[4..12]);
    time_bytes[0] ^= random_part[0];
    time_bytes[1] ^= random_part[1];
    time_bytes[2] ^= random_part[2];
    time_bytes[3] ^= random_part[3];
    time_bytes[4] ^= random_part[0];
    time_bytes[5] ^= random_part[1];
    time_bytes[6] ^= random_part[2];
    time_bytes[7] ^= random_part[3];
    let now = SystemTime::now().duration_since(UNIX_EPOCH)
        .map_err(|e| Error::new(ErrorKind::Other, e))?.as_secs();
    let time = u64::from_le_bytes(time_bytes);
    if (now >= time && now - time > MAX_TIME_DIFFERENCE) ||
        (now < time && time - now > MAX_TIME_DIFFERENCE) {
        return Err(Error::new(ErrorKind::InvalidData, "incorrect nonce"));
    }
    Ok(())
}

fn build_iv() -> Result<[u8; 12], Error> {
    let mut random_part = [0u8; 4];
    OsRng.try_fill_bytes(&mut random_part)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let mut iv = [0u8; 12];
    let now = SystemTime::now().duration_since(UNIX_EPOCH)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    iv[0..4].copy_from_slice(&random_part);
    let mut time_bytes = now.as_secs().to_le_bytes();
    time_bytes[0] ^= random_part[0];
    time_bytes[1] ^= random_part[1];
    time_bytes[2] ^= random_part[2];
    time_bytes[3] ^= random_part[3];
    time_bytes[4] ^= random_part[0];
    time_bytes[5] ^= random_part[1];
    time_bytes[6] ^= random_part[2];
    time_bytes[7] ^= random_part[3];
    iv[4..12].copy_from_slice(&time_bytes);
    Ok(iv)
}

pub fn build_message_processor(key: [u8; 32], db: DB)
    -> Result<Arc<dyn MessageProcessor + Sync + Send>, Error> {
    Ok(Arc::new(UserMessageProcessor::new(key, db)?))
}
