use std::io::{Error, ErrorKind, Read};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use bzip2::Compression;
use bzip2::read::BzEncoder;
use chacha20::ChaCha20;
use chacha20::cipher::{KeyIvInit, StreamCipher};
use rand::TryRngCore;
use rand::rngs::OsRng;
use crate::base_server::MessageProcessor;
use crate::logger::Logger;

const MAX_TIME_DIFFERENCE: u64 = 60;

pub trait CommandProcessor {
    fn get_message_prefix_length(&self) -> usize;
    fn get_key(&self, message_prefix: &[u8]) -> Result<[u8; 32], Error>;
    fn check_message_length(&self, length: usize) -> bool;
    fn execute(&self, command: Vec<u8>, message_prefix: &[u8]) -> Result<Vec<u8>, Error>;
}

struct UserMessageProcessor {
    command_processor: Box<dyn CommandProcessor + Send + Sync>,
    use_bzip2: bool
}

impl UserMessageProcessor {
    fn new(command_processor: Box<dyn CommandProcessor + Send + Sync>, use_bzip2: bool)
        -> Result<UserMessageProcessor, Error> {
        Ok(UserMessageProcessor {command_processor, use_bzip2})
    }
}

impl MessageProcessor for UserMessageProcessor {
    fn process_message(&self, logger: &Logger, message: &Vec<u8>, _time_offset: i64) -> Vec<u8> {
        let message_prefix_length = self.command_processor.get_message_prefix_length();
        if message.len() < 13 + message_prefix_length ||
            !self.command_processor.check_message_length(message.len() - 12 - message_prefix_length) {
            logger.error("Wrong message length");
            return Vec::new();
        }
        let message_prefix = &message[..message_prefix_length];
        let key = match self.command_processor.get_key(message_prefix) {
            Ok(key) => key,
            Err(e) => {
                logger.error(format!("get_key error: {}", e));
                return Vec::new()
            }
        };
        let result = match self.process_message_with_result(logger, 
                                                            &message[message_prefix_length..],
                                                            &key, message_prefix) {
            Ok(message) => message,
            Err(error) => {
                logger.error(format!("process_message error: {}", error));
                let mut message = Vec::new();
                message.push(2); // error
                message.extend_from_slice(error.to_string().as_bytes());
                message
            }
        };
        match self.encrypt_response(result, &key) {
            Ok(encrypted) => encrypted,
            Err(error) => {
                logger.error(format!("encrypt_response error: {}", error));
                Vec::new()
            }
        }
    }
}

impl UserMessageProcessor {
    fn process_message_with_result(&self, logger: &Logger, message: &[u8], key: &[u8; 32],
                                   message_prefix: &[u8])
        -> Result<Vec<u8>, Error> {
        match decrypt(message, &key) {
            Ok(command) => self.command_processor.execute(command, message_prefix),
            Err(error) => {
                logger.error(format!("message decrypt error: {}", error));
                Ok(Vec::new())
            }
        }
    }
    
    fn encrypt_response(&self, response: Vec<u8>, key: &[u8; 32]) -> Result<Vec<u8>, Error> {
        let compressed = if self.use_bzip2 {compress(response)?} else {response};
        encrypt(compressed, key)
    }
}

fn encrypt(data: Vec<u8>, key: &[u8; 32]) -> Result<Vec<u8>, Error> {
    let iv_raw = build_iv()?;
    let mut iv = encrypt_iv(&iv_raw, key)?;
    let bytes = data.as_slice();
    let mut out_vec = transform(key, &iv_raw, bytes)?;
    iv.append(&mut out_vec);
    Ok(iv)
}

fn decrypt(message: &[u8], key: &[u8; 32]) -> Result<Vec<u8>, Error> {
    let iv = &message[0..12];
    let decrypted_iv_vec = encrypt_iv(iv, key)?;
    let decrypted_iv = decrypted_iv_vec.as_slice();
    check_iv(decrypted_iv)?;
    let encrypted = &message[12..];
    transform(key, decrypted_iv, encrypted)
}

fn encrypt_iv(iv: &[u8], key: &[u8; 32]) -> Result<Vec<u8>, Error> {
    let random_part = &iv[0..4];
    let mut iv3 = [0u8; 12];
    iv3[0..4].copy_from_slice(random_part);
    iv3[4..8].copy_from_slice(random_part);
    iv3[8..12].copy_from_slice(random_part);
    let transformed = transform(key, &iv3, &iv[4..12])?;
    let mut result = Vec::from(random_part);
    result.extend_from_slice(&transformed);
    Ok(result)
}

fn transform(key: &[u8; 32], iv: &[u8], data: &[u8]) -> Result<Vec<u8>, Error> {
    let mut cipher = ChaCha20::new(key.into(), iv.into());
    let mut out_vec = vec![0u8; data.len()];
    let out_bytes = out_vec.as_mut_slice();
    cipher.apply_keystream_b2b(data, out_bytes)
        .map_err(|e| Error::new(ErrorKind::Other, e.to_string()))?;
    Ok(out_vec)
}

fn compress(data: Vec<u8>) -> Result<Vec<u8>, Error> {
    let mut compressor = BzEncoder::new(data.as_slice(), Compression::best());
    let mut result = Vec::new();
    compressor.read_to_end(&mut result)?;
    Ok(result)
}

fn check_iv(iv: &[u8]) -> Result<(), Error> {
    let mut time_bytes = [0u8; 8];
    time_bytes.copy_from_slice(&iv[4..12]);
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
    let time_bytes = now.as_secs().to_le_bytes();
    iv[4..12].copy_from_slice(&time_bytes);
    Ok(iv)
}

pub fn build_message_processor(command_processor: Box<dyn CommandProcessor + Send + Sync>,
                               use_bzip2: bool)
    -> Result<Arc<dyn MessageProcessor + Sync + Send>, Error> {
    Ok(Arc::new(UserMessageProcessor::new(command_processor, use_bzip2)?))
}

#[cfg(test)]
mod tests {
    use std::io::{Error, ErrorKind};
    use rand::rngs::OsRng;
    use rand::TryRngCore;
    use crate::user_message_processor::{build_iv, check_iv, decrypt, encrypt, encrypt_iv};

    #[test]
    fn test_iv() -> Result<(), Error> {
        let mut key = [0u8; 32];
        OsRng.try_fill_bytes(&mut key)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let iv_raw = build_iv()?;
        check_iv(&iv_raw)?;
        let iv = encrypt_iv(&iv_raw, &key)?;
        let decrypted_iv = encrypt_iv(iv.as_slice(), &key)?;
        check_iv(decrypted_iv.as_slice())?;
        assert_eq!(iv_raw, decrypted_iv.as_slice());
        Ok(())
    }

    #[test]
    fn test_encrypt_iv() -> Result<(), Error> {
        let key = [0u8, 1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8,
                                13u8, 14u8, 15u8, 16u8, 17u8, 18u8, 19u8, 20u8, 21u8, 22u8, 23u8,
                                24u8, 25u8, 26u8, 27u8, 28u8, 29u8, 30u8, 31u8];
        let iv_raw = [1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8];
        let iv = encrypt_iv(&iv_raw, &key)?;
        assert_eq!(iv.as_slice(), [1, 2, 3, 4, 87, 191, 4, 40, 131, 151, 75, 156]);
        let decrypted_iv = encrypt_iv(iv.as_slice(), &key)?;
        assert_eq!(iv_raw, decrypted_iv.as_slice());
        Ok(())
    }

    #[test]
    fn test_encrypt_decrypt() -> Result<(), Error> {
        let key = [0u8, 1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8,
            13u8, 14u8, 15u8, 16u8, 17u8, 18u8, 19u8, 20u8, 21u8, 22u8, 23u8,
            24u8, 25u8, 26u8, 27u8, 28u8, 29u8, 30u8, 31u8];
        let message = vec![1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8];
        let encrypted = encrypt(message.clone(), &key)?;
        let decrypted = decrypt(&encrypted, &key)?;
        assert_eq!(message, decrypted);
        Ok(())
    }
}
