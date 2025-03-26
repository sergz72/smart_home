use smart_home_common::db::DB;

pub struct CommandProcessor {
    db: DB
}

impl CommandProcessor {
    pub fn new(db: DB) -> CommandProcessor {
        CommandProcessor { db }
    }

    pub fn execute(&self, command: Vec<u8>) -> Vec<u8> {
        todo!()
    }
}