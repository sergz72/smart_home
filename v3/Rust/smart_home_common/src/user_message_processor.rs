use std::io::{Error, ErrorKind, Read};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};
use bzip2::Compression;
use bzip2::read::BzEncoder;
use chacha20::ChaCha20;
use chacha20::cipher::{KeyIvInit, StreamCipher};
use rand::TryRng;
use rand::rngs::SysRng;
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
    use_bzip2: bool,
    transform_iv_on_encrypt: bool,
    transform_iv_on_decrypt: bool
}

impl UserMessageProcessor {
    fn new(command_processor: Box<dyn CommandProcessor + Send + Sync>, use_bzip2: bool,
           transform_iv_on_encrypt: bool, transform_iv_on_decrypt: bool)
        -> Result<UserMessageProcessor, Error> {
        Ok(UserMessageProcessor {command_processor, use_bzip2, transform_iv_on_encrypt, transform_iv_on_decrypt})
    }

    fn encrypt_response(&self, response: Vec<u8>, key: &[u8; 32]) -> Result<Vec<u8>, Error> {
        let compressed = if self.use_bzip2 {compress(response)?} else {response};
        self.encrypt(compressed, key)
    }

    fn encrypt(&self, data: Vec<u8>, key: &[u8; 32]) -> Result<Vec<u8>, Error> {
        let iv_raw = if self.transform_iv_on_encrypt {build_iv()? } else { build_random_iv()? };
        let iv = if self.transform_iv_on_encrypt { encrypt_iv(&iv_raw, key)? } else { iv_raw };
        let mut result = vec![0u8; 12 + data.len()];
        result[..12].copy_from_slice(&iv);
        transform(key, &iv_raw, &data, &mut result[12..]);
        Ok(result)
    }

    fn decrypt(&self, message: &[u8], key: &[u8; 32]) -> Result<Vec<u8>, Error> {
        let iv = &message[0..12];
        let decrypted_iv = if self.transform_iv_on_decrypt {
            let encrypted_iv = encrypt_iv(iv, key)?;
            check_iv(&encrypted_iv)?;
            encrypted_iv
        } else { iv.try_into().unwrap() };
        let encrypted = &message[12..];
        let mut result = vec![0u8; message.len() - 12];
        transform(key, &decrypted_iv, encrypted, &mut result);
        Ok(result)
    }
}

impl MessageProcessor for UserMessageProcessor {
    fn process_message(&self, logger: &Logger, message: &Vec<u8>) -> Vec<u8> {
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
        let command = match self.decrypt(&message[message_prefix_length..], &key) {
            Ok(command) => command,
            Err(error) => {
                logger.error(format!("message decrypt error: {}", error));
                return Vec::new()
            }
        };
        let result = match self.command_processor.execute(command, message_prefix) {
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

fn encrypt_iv(iv: &[u8], key: &[u8; 32]) -> Result<[u8; 12], Error> {
    let random_part = &iv[0..4];
    let mut iv3 = [0u8; 12];
    iv3[0..4].copy_from_slice(random_part);
    iv3[4..8].copy_from_slice(random_part);
    iv3[8..12].copy_from_slice(random_part);
    let mut result = [0u8; 12];
    result[..4].copy_from_slice(&random_part);
    transform(key, &iv3, &iv[4..12], &mut result[4..]);
    Ok(result)
}

fn transform(key: &[u8; 32], iv: &[u8; 12], data: &[u8], out: &mut [u8]) {
    let mut cipher = ChaCha20::new(key.into(), iv.into());
    cipher.apply_keystream_b2b(data, out);
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
    SysRng.try_fill_bytes(&mut random_part)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    let mut iv = [0u8; 12];
    let now = SystemTime::now().duration_since(UNIX_EPOCH)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    iv[0..4].copy_from_slice(&random_part);
    let time_bytes = now.as_secs().to_le_bytes();
    iv[4..12].copy_from_slice(&time_bytes);
    Ok(iv)
}

fn build_random_iv() -> Result<[u8; 12], Error> {
    let mut iv = [0u8; 12];
    SysRng.try_fill_bytes(&mut iv)
        .map_err(|e| Error::new(ErrorKind::Other, e))?;
    Ok(iv)
}

pub fn build_message_processor(command_processor: Box<dyn CommandProcessor + Send + Sync>,
                               use_bzip2: bool, transform_iv_on_encrypt: bool, transform_iv_on_decrypt: bool)
    -> Result<Arc<dyn MessageProcessor + Sync + Send>, Error> {
    Ok(Arc::new(UserMessageProcessor::new(command_processor, use_bzip2, transform_iv_on_encrypt, transform_iv_on_decrypt)?))
}

#[cfg(test)]
mod tests {
    use std::io::{Error, ErrorKind};
    use rand::rngs::SysRng;
    use rand::TryRng;
    use crate::user_message_processor::{build_iv, check_iv, encrypt_iv, CommandProcessor, UserMessageProcessor};

    struct TestCommandProcessor{}

    impl CommandProcessor for TestCommandProcessor {
        fn get_message_prefix_length(&self) -> usize {
            todo!()
        }

        fn get_key(&self, _: &[u8]) -> Result<[u8; 32], Error> {
            todo!()
        }

        fn check_message_length(&self, _: usize) -> bool {
            todo!()
        }

        fn execute(&self, _: Vec<u8>, _: &[u8]) -> Result<Vec<u8>, Error> {
            todo!()
        }
    }

    #[test]
    fn test_iv() -> Result<(), Error> {
        let mut key = [0u8; 32];
        SysRng.try_fill_bytes(&mut key)
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
    fn test_encrypt_decrypt_with_transform() -> Result<(), Error> {
        let message_processor =
            UserMessageProcessor::new(Box::new(TestCommandProcessor{}), false, true, true)?;
        let key = [0u8, 1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8,
            13u8, 14u8, 15u8, 16u8, 17u8, 18u8, 19u8, 20u8, 21u8, 22u8, 23u8,
            24u8, 25u8, 26u8, 27u8, 28u8, 29u8, 30u8, 31u8];
        let message = vec![1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8];
        let encrypted = message_processor.encrypt(message.clone(), &key)?;
        let decrypted = message_processor.decrypt(&encrypted, &key)?;
        assert_eq!(message, decrypted);
        Ok(())
    }

    #[test]
    fn test_encrypt_decrypt_without_transform() -> Result<(), Error> {
        let message_processor =
            UserMessageProcessor::new(Box::new(TestCommandProcessor{}), false, false, false)?;
        let key = [0u8, 1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8,
            13u8, 14u8, 15u8, 16u8, 17u8, 18u8, 19u8, 20u8, 21u8, 22u8, 23u8,
            24u8, 25u8, 26u8, 27u8, 28u8, 29u8, 30u8, 31u8];
        let message = vec![1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8, 11u8, 12u8];
        let encrypted = message_processor.encrypt(message.clone(), &key)?;
        let decrypted = message_processor.decrypt(&encrypted, &key)?;
        assert_eq!(message, decrypted);
        Ok(())
    }
}
