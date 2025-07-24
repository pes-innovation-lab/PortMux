mod connection;
mod protocol;

use crate::connection::handle_connection;
use crate::protocol::find_protocol;
use tokio::io::AsyncReadExt;
use tokio::net::TcpListener;

#[tokio::main]
async fn main() {
    let client_listener = TcpListener::bind("0.0.0.0:8080").await.unwrap();
    loop {
        match client_listener.accept().await {
            Ok((mut client_socket, addr)) => {
                println!("New connection from: {}", addr);

                tokio::spawn(async move {
                    let mut buffer = vec![0; 4096];
                    let n = match client_socket.read(&mut buffer).await {
                        Ok(n) if n > 0 => n,
                        Ok(_) => {
                            println!("Client closed connection immediately");
                            return;
                        }
                        Err(err) => {
                            eprintln!("Failed to read from client: {}", err);
                            return;
                        }
                    };

                    let buffer = buffer[..n].to_vec();
                    println!("Received {} bytes", buffer.len());

                    match find_protocol(&buffer) {
                        Some(protocol) => {
                            println!(
                                "Detected protocol: {} -> port {}",
                                protocol.name, protocol.port
                            );
                            handle_connection(client_socket, protocol, buffer).await;
                        }
                        None => {
                            eprintln!("Unknown protocol, closing connection");
                            return;
                        }
                    }
                });
            }
            Err(e) => {
                eprintln!("Failed to accept connection: {}", e);
            }
        }
    }
}
