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
    fn process_message(&self, logger: &Logger, message: &Vec<u8>, time_offset: i64) -> Vec<u8> {
        let command = self.decrypt(logger, message);
        if command.is_empty() {
            return Vec::new();
        }
        let response = self.command_processor.execute(command);
        self.encrypt_response(response)
    }
}

impl UserMessageProcessor {
    fn decrypt(&self, logger: &Logger, message: &Vec<u8>) -> Vec<u8> {
        todo!()
    }

    fn encrypt_response(&self, response: Vec<u8>) -> Result<Vec<u8>, Error> {
        let compressed = self.compress(response)?;
        self.encrypt(compressed)
    }

    fn compress(&self, data: Vec<u8>) -> Result<Vec<u8>, Error> {
        let mut compressor = BzEncoder::new(data.as_slice(), Compression::best());
        let mut result = Vec::new();
        compressor.read_to_end(&mut result)?;
        Ok(result)
    }

    fn encrypt(&self, data: Vec<u8>) -> Result<Vec<u8>, Error> {
        let iv = self.build_iv()?;
        let mut cipher = ChaCha20::new((&self.key).into(), (&iv).into());
        let bytes = data.as_slice();
        let mut out_vec = vec![0u8; bytes.len()];
        let out_bytes = out_vec.as_mut_slice();
        cipher.apply_keystream_b2b(bytes, out_bytes)
            .map_err(|e| Error::new(ErrorKind::Other, e.to_string()))?;
        let mut result = Vec::from(iv);
        result.append(&mut out_vec);
        Ok(result)
    }

    fn build_iv(&self) -> Result<[u8; 12], Error> {
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
}

pub fn build_message_processor(key: [u8; 32], db: DB)
    -> Result<Arc<dyn MessageProcessor + Sync + Send>, Error> {
    Ok(Arc::new(UserMessageProcessor::new(key, db)?))
}
