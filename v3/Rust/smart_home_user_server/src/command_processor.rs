use std::io::{Error, ErrorKind};
use smart_home_common::db::Database;
use smart_home_common::entities::{add_string_to_binary, SensorDataQuery};
use smart_home_common::user_message_processor::CommandProcessor;

pub struct UserCommandProcessor {
    db: Box<dyn Database + Send + Sync>,
    key: [u8; 32],
    locations: Vec<u8>
}

impl CommandProcessor for UserCommandProcessor {
    fn get_message_prefix_length(&self) -> usize {
        0
    }

    fn get_key(&self, _message_prefix: &[u8]) -> Result<[u8; 32], Error> {
        Ok(self.key.clone())
    }
    
    fn check_message_length(&self, length: usize) -> bool {
        length == 12 || length == 1
    }

    fn execute(&self, command: Vec<u8>, _message_prefix: &[u8]) -> Result<Vec<u8>, Error> {
        match command[0] {
            0 => self.execute_locations_query(command.len()),
            1 => self.execute_last_data_query(command.len()),
            2 => self.execute_sensor_data_query(command[1..].to_vec()),
            _ => Err(Error::new(ErrorKind::InvalidInput, "Invalid command"))
        }
    }
}

impl UserCommandProcessor {
    pub fn new(db: Box<dyn Database + Send + Sync>, key: [u8; 32]) -> Result<Box<UserCommandProcessor>, Error> {
        let locations = build_locations(&db)?;
        Ok(Box::new(UserCommandProcessor { db, key, locations }))
    }

    fn execute_locations_query(&self, command_length: usize) -> Result<Vec<u8>, Error> {
        if command_length != 1 {
            return Err(Error::new(ErrorKind::InvalidInput, "Invalid locations command length"));
        }
        Ok(self.locations.clone())
    }

    fn execute_last_data_query(&self, command_length: usize) -> Result<Vec<u8>, Error> {
        if command_length != 1 {
            return Err(Error::new(ErrorKind::InvalidInput, "Invalid last data command length"));
        }
        let data = self.db.get_last_sensor_data()?;
        let binary = data.to_binary()?;
        Ok(binary)
    }

    pub fn execute_sensor_data_query(&self, command: Vec<u8>) -> Result<Vec<u8>, Error> {
        let result = self.db.get_sensor_data(SensorDataQuery::from_binary(command)?)?;
        Ok(result.to_binary())
    }
}

fn build_locations(database: &Box<dyn Database + Send + Sync>) -> Result<Vec<u8>, Error> {
    let time_zone = database.get_time_zone();
    let mut result = Vec::new();
    add_string_to_binary(&mut result, &time_zone);
    for (location_id, location) in database.get_locations()? {
        result.push(location_id as u8);
        location.to_binary(&mut result)?;
    }
    Ok(result)
}

#[cfg(test)]
mod tests {
    /*use std::io::Error;
    use crate::command_processor::UserCommandProcessor;

    #[test]
    fn test_get_sensor_data() -> Result<(), Error> {
        let processor = UserCommandProcessor::new(
            DB::new("postgresql://postgres@localhost/smart_home".to_string()), 0,
            [0u8; 32]
        );
        let (result, aggregated) = processor.get_sensor_data(
            SensorDataQuery{max_points: 200, data_type: "env".to_string(),
                                    date_or_offset: 20250301, offset_unit: 0, period: 0, period_unit: 0},
        )?;
        assert_eq!(true, aggregated);
        Ok(())
    }

    #[test]
    fn test_get_sensors() -> Result<(), Error> {
        let processor = UserCommandProcessor::new(
            DB::new("postgresql://postgres@localhost/smart_home".to_string()), 0,
            [0u8; 32]
        );
        let result = processor.get_sensors()?;
        assert_eq!(result.len(), 8);
        Ok(())
    }

    #[test]
    fn test_get_last_sensor_data() -> Result<(), Error> {
        let processor = UserCommandProcessor::new(
            DB::new("postgresql://postgres@localhost/smart_home".to_string()), 0,
            [0u8; 32]
        );
        let result = processor.get_last_sensor_data(20250301)?;
        assert_eq!(result.len(), 4);
        Ok(())
    }*/
}