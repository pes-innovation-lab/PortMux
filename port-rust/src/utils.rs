use serde_yml::Value;
use std::fs;

pub fn read_config() -> Value {
    let content = fs::read_to_string("config.yaml").expect("Failed to read config.yaml");
    serde_yml::from_str(&content).expect("Failed to parse YAML")
}
