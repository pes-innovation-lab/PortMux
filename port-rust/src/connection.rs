use crate::protocol::Protocol;

use tokio::io::{AsyncWriteExt, copy_bidirectional};
use tokio::net::TcpStream;

pub async fn handle_connection(mut client_socket: TcpStream, protocol: Protocol, buffer: Vec<u8>) {
    match TcpStream::connect(format!("127.0.0.1:{}", protocol.port)).await {
        Ok(mut service_socket) => {
            println!("Connected to service on port {}", protocol.port);
            if let Err(err) = service_socket.write_all(&buffer).await {
                eprintln!("Failed to write initial buffer to service: {}", err);
                return;
            }

            println!("Starting bidirectional copy...");
            match copy_bidirectional(&mut client_socket, &mut service_socket).await {
                Ok(_) => {
                    println!("Client Disconnected")
                } // taking client socket and service socket connection and letting connnection sit indefinitely, until disconnection
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
