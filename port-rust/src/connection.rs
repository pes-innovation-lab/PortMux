use crate::protocol::Protocol;

use tokio::io::{AsyncWriteExt, copy_bidirectional_with_sizes};
use tokio::net::TcpStream;

//give user the ability to pick where to bind

pub async fn handle_connection(mut client_socket: TcpStream, protocol: Protocol, buffer: Vec<u8>) {
    let a_b_buffer: usize;
    let b_a_buffer: usize;
    if protocol.priority == "latency"{a_b_buffer = 1024; b_a_buffer = 1024;}
    else if protocol.priority == "throughput"{a_b_buffer = 32768; b_a_buffer = 32768;}
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
