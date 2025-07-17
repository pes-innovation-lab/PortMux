use serde_yml::{Value};
use std::fs;

fn main() {
    let data: Value = serde_yml::from_str(&fs::read_to_string("config.yaml").unwrap()).unwrap();
    println!("{:#?}", data["http"]);
if let Some(http) = data["http"].as_mapping() {
        for (key, value) in http {
            if let Some(k) = key.as_str() {
                println!("Key: {}", k);
            }
        }
    }
}