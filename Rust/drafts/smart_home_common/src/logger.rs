use std::fmt::Display;
use chrono::Local;

pub struct Logger {
    target: String
}

impl Logger {
    pub fn new(target: String) -> Logger {
        Logger{target}
    }

    fn log<S: Into<String> + Display>(&self, level: &str, message: S) {
        println!("{} {} {}: {}", Local::now().format("%Y-%m-%d %H:%M:%S%.3f"), self.target,
                 level, message);
    }
    
    pub fn info<S: Into<String> + Display>(&self, message: S) {
        self.log("INFO", message);
    }

    pub fn error<S: Into<String> + Display>(&self, message: S) {
        self.log("ERROR", message);
    }
}

#[cfg(test)]
mod tests {
    use crate::logger::Logger;

    #[test]
    fn test_logger() {
        let logger = Logger::new("connection1".to_string());
        logger.info("hello info".to_string());
        logger.error("hello error".to_string());
    }
}
