use crate::protocol::types::Protocol;
use tokio::io::{AsyncWriteExt, copy_bidirectional};
use tokio::net::TcpStream;

pub async fn handle_connection(mut client_socket: TcpStream, protocol: Protocol, buffer: Vec<u8>) {
    let (a_b_buffer, b_a_buffer) = match protocol.priority.as_str() {
        "latency" => (1024, 1024),
        "throughput" => (32768, 32768),
        _ => (8192, 8192),
    };

    match TcpStream::connect(format!("127.0.0.1:{}", protocol.port)).await {
        Ok(mut service_socket) => {
            if let Err(err) = service_socket.write_all(&buffer).await {
                eprintln!("Failed to write to service: {}", err);
                return;
            }

            match copy_bidirectional(&mut client_socket, &mut service_socket).await {
                Ok(_) => println!("Client disconnected."),
                Err(err) => eprintln!("Data copy error: {}", err),
            }
        }
        Err(err) => eprintln!("Failed to connect to service: {}", err),
    }
}
