use std::collections::HashMap;
use std::io::Error;
use std::sync::{Arc, Mutex};
use aes::Aes128;
use aes::cipher::generic_array::GenericArray;
use aes::cipher::{BlockDecrypt, KeyInit};
use aes::cipher::typenum::U16;
use smart_home_common::base_server::MessageProcessor;
use smart_home_common::keys::read_key_file16;
use log::error;
use crate::db::DeviceSensor;

const DEVICE_DATA_OFFSETS: [usize; 10] = [10, 13, 20, 23, 26, 29, 36, 39, 42, 45];

struct SensorMessageProcessor {
    device_sensors: HashMap<usize, HashMap<usize, DeviceSensor>>,
    aes: Aes128,
    last_device_time: Mutex<HashMap<u16, u32>>,
    key_sum: u32
}

impl SensorMessageProcessor {
    fn new(key_file_name: &String, device_sensors: HashMap<usize, HashMap<usize, DeviceSensor>>)
        -> Result<SensorMessageProcessor, Error> {
        let key_bytes = read_key_file16(key_file_name)?;
        let mut key_sum: u32 = 0;
        let mut n = 0;
        while n < key_bytes.len() {
            let mut buf32 = [0u8; 4];
            buf32.copy_from_slice(&key_bytes[n..n+4]);
            key_sum += u32::from_le_bytes(buf32);
            n += 4;
        }
        let key = GenericArray::from(key_bytes);
        let aes = Aes128::new(&key);
        Ok(SensorMessageProcessor {device_sensors, aes,
            last_device_time: Mutex::new(HashMap::new()), key_sum})
    }
}

impl MessageProcessor for SensorMessageProcessor {
    fn process_message(&self, message: &Vec<u8>) -> Vec<u8> {
        let n = message.len();
        if n != 16 && n != 32 && n != 48 {
            error!("wrong data size");
            return Vec::new();
        }

        let decrypted = self.decrypt(message);
        let mut buf16 = [0u8; 2];
        buf16.copy_from_slice(&decrypted[4..6]);
        let device_id = u16::from_le_bytes(buf16);
        match self.get_sensors(device_id) {
            Some(sensors) => 
                self.process_decrypted_message(device_id, decrypted, sensors),
            None => error!("wrong sensor id")
        }
        Vec::new()
    }
}

impl SensorMessageProcessor {
    fn process_decrypted_message(&self, device_id: u16, decrypted: Vec<u8>,
                                 sensors: &HashMap<usize, DeviceSensor>) {
        // crc check
        let mut buf32 = [0u8; 4];
        buf32.copy_from_slice(&decrypted[0..4]);
        let crc = u32::from_le_bytes(buf32);
        if crc != self.calculate_crc(&decrypted) {
            error!("wrong CRC");
            return;
        }
        // event time check
        buf32.copy_from_slice(&decrypted[6..10]);
        let event_time = u32::from_le_bytes(buf32);
        let l = decrypted.len();
        let mut n = 16;
        while l > n {
            buf32.copy_from_slice(&decrypted[n..n+4]);
            let event_time2 = u32::from_le_bytes(buf32);
            if event_time != event_time2 {
                error!("wrong event_time{n}");
                return;
            }
            n += 16;
        }
        if event_time < *self.last_device_time.lock().unwrap().get(&device_id).unwrap_or(&0) {
            error!("wrong event_time");
            return;
        }
        //todo
        self.last_device_time.lock().unwrap().insert(device_id, event_time);
    }
    
    fn calculate_crc(&self, decrypted: &Vec<u8>) -> u32 {
        let mut crc: u32 = 0;
        let mut n = 0;
        while n < decrypted.len() {
            let mut buf32 = [0u8; 4];
            buf32.copy_from_slice(&decrypted[n..n+4]);
            crc += u32::from_le_bytes(buf32);
            n += 4;
        }
        crc * self.key_sum
    }

    fn get_sensors(&self, device_id: u16) -> Option<&HashMap<usize, DeviceSensor>> {
        let did = device_id as usize;
        self.device_sensors.get(&did)
    }

    fn decrypt(&self, message: &Vec<u8>) -> Vec<u8> {
        let mut result = Vec::new();
        let mut n = 0;
        let mut l = message.len();
        while l != 0 {
            let mut block: GenericArray<u8, U16> = GenericArray::clone_from_slice(&message[n..n+16]);
            self.aes.decrypt_block(&mut block);
            result.extend_from_slice(block.as_slice());
            n += 16;
            l -= 16;
        }
        result
    }
}

pub fn build_message_processor(device_key_file_name: &String, device_sensors: HashMap<usize, HashMap<usize, DeviceSensor>>)
    -> Result<Arc<dyn MessageProcessor + Sync + Send>, Error> {
    Ok(Arc::new(SensorMessageProcessor::new(device_key_file_name, device_sensors)?))
}