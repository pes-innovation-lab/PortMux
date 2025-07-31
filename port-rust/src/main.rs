mod connection;
mod protocol;

use crate::connection::handle_connection;
use crate::protocol::find_protocol;

use tokio::io::AsyncReadExt;
use tokio::net::TcpListener;
use std::sync::Arc;
use std::fs;
use serde_yml::Value;

async fn load_config() -> Result<Arc<Value>, Box<dyn std::error::Error>> {
    let content = fs::read_to_string("config.yaml")?;
    let yaml: Value = serde_yml::from_str(&content)?;
    Ok(Arc::new(yaml))
}

#[tokio::main]
async fn main() {
    let config = match load_config().await {
        Ok(cfg) => cfg,
        Err(e) => {
            eprintln!("Error loading config.yaml: {}", e);
            std::process::exit(1);
        }
    };
    let (ip, port) = match &config["PORTMUX"] {
        Value::Mapping(portmux) => (
            portmux.get("ip").and_then(Value::as_str).unwrap_or("0.0.0.0").to_string(),
            portmux.get("port").and_then(Value::as_u64).unwrap_or(8080),
        ),
        _ => ("0.0.0.0".to_string(), 8080),
    };
    let client_listener = TcpListener::bind(format!("{}:{}",ip,port)).await.unwrap();
    
    loop {
    match client_listener.accept().await {
        Ok((mut client_socket, _addr)) => {
            let config = Arc::clone(&config);

            tokio::spawn(async move {
                let mut buffer = vec![0; 4096];
                match client_socket.read(&mut buffer).await {
                    Ok(n) if n > 0 => {
                        buffer.truncate(n);

                        match find_protocol(&buffer, &config) {
                            Some(protocol) => {
                                if let Err(e) = handle_connection(client_socket, protocol, buffer).await {
                                    eprintln!("Connection handling error: {}", e);
                                }
                            }
                            None => {
                                eprintln!("Unknown protocol detected, closing connection");
                            }
                        }
                    }
                    Ok(_) => {},
                    Err(e) => eprintln!("Error reading from client: {}", e),
                }
            });
        }
        Err(e) => eprintln!("Failed to accept connection: {}", e),
    }
}
}
