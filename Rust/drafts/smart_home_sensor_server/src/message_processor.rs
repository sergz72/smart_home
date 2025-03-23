use std::sync::Arc;
use smart_home_common::base_server::MessageProcessor;

pub fn build_message_processor(device_key_file_name: &String) -> Arc<dyn MessageProcessor + Sync + Send> {
    todo!()
}