mod connection;
mod protocol;

use crate::connection::handle_connection;
use crate::protocol::find_protocol;

use tokio::io::AsyncReadExt;
use tokio::net::TcpListener;
use std::sync::Arc;
use std::fs;
use serde_yml::Value;


#[tokio::main]
async fn main() {
    let config = Arc::new(serde_yml::from_str::<Value>(&fs::read_to_string("config.yaml").expect("Failed to read config.yaml")).expect("Failed to parse YAML"));
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
            Ok((mut client_socket, addr)) => {
                println!("New connection from: {}", addr);
                let config = Arc::clone(&config);

                tokio::spawn(async move {
                    let mut buffer = vec![0; 4096];
                    let n = match client_socket.read(&mut buffer).await {
                        Ok(n) if n > 0 => n,
                        _ => {
                            println!("Client disconnected or error");
                            return;
                        }
                    };

                    let buffer = buffer[..n].to_vec();
                    println!("Received {} bytes", buffer.len());

                    match find_protocol(&buffer, &config) {
                        Some(protocol) => {
                            println!("Detected protocol: {} -> port {}", protocol.name, protocol.port);
                            handle_connection(client_socket, protocol, buffer).await;
                        }
                        None => {
                            eprintln!("Unknown protocol, closing connection");
                        }
                    }
                });
            }
            Err(e) => eprintln!("Failed to accept connection: {}", e),
        }
    }
}
