mod connection;
mod protocol;

use crate::protocol::find_protocol;
use tokio::io::{AsyncReadExt};
use tokio::net::{TcpListener};

use crate::protocol::Protocol;

use tokio::io::{AsyncWriteExt, copy_bidirectional_with_sizes};
use tokio::net::{TcpStream};


pub async fn handle_connection(mut client_socket: TcpStream, protocol: Protocol, buffer: Vec<u8>) {
    let a_b_buffer: usize;
    let b_a_buffer: usize;
    if (protocol.priority == "latency"){a_b_buffer = 1024; b_a_buffer = 1024;}
    else if (protocol.priority == "throughput"){a_b_buffer = 32768; b_a_buffer = 32768;}
    else {a_b_buffer = 8192; b_a_buffer = 8192;}


    match TcpStream::connect(format!("127.0.0.1:{}", protocol.port)).await {
        Ok(mut service_socket) => {
            println!("Connected to service on port {}", protocol.port);

            if let Err(err) = service_socket.write_all(&buffer).await {
                eprintln!("Failed to write initial buffer to service: {}", err);
                return;
            }

            println!("Starting bidirectional copy...");
            match copy_bidirectional_with_sizes(&mut client_socket, &mut service_socket, a_b_buffer, b_a_buffer).await {  
                Ok(_) => {
                    println!("Client Disconnected")
                }, // taking client socket and service socket connection and letting connnection sit indefinitely, until disconnection
                Err(err) => {
                    eprintln!("Failed to copy data from client_socket to service: {}", err);
                }
            }
            println!("Bidirectional copy finished.");
        }
        Err(err) => {
            eprintln!("Failed to connect to service: {}", err);
        }
    }
}

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
