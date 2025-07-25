mod connection;
mod protocol;
mod utils;

use crate::connection::handle_connection;
use crate::protocol::find_protocol;
use crate::utils::read_config;

use tokio::io::AsyncReadExt;
use tokio::net::TcpListener;
use std::sync::Arc;

#[tokio::main]
async fn main() {
    let client_listener = TcpListener::bind("0.0.0.0:8080").await.unwrap();
    let config = Arc::new(read_config());

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
