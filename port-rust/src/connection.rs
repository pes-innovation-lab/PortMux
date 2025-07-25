use crate::protocol::Protocol;
use tokio::io::{AsyncWriteExt, copy_bidirectional_with_sizes};
use tokio::net::TcpStream;

static LATENCY_BUFFER_SIZE:u32 = 1024;
static THROUGHPUT_BUFFER_SIZE:u32 = 32768;
static DEFAULT_BUFFER_SIZE:u32 = 8192;

pub async fn handle_connection(mut client_socket: TcpStream, protocol: Protocol, buffer: Vec<u8>) {
    let (a_b_buffer, b_a_buffer) = match protocol.priority.as_str() {
        "latency" => (LATENCY_BUFFER_SIZE, LATENCY_BUFFER_SIZE),
        "throughput" => (THROUGHPUT_BUFFER_SIZE, THROUGHPUT_BUFFER_SIZE),
        _ => (DEFAULT_BUFFER_SIZE, DEFAULT_BUFFER_SIZE),
    };

    match TcpStream::connect(format!("127.0.0.1:{}", protocol.port)).await {
        Ok(mut service_socket) => {
            if let Err(err) = service_socket.write_all(&buffer).await {
                eprintln!("Failed to write to service: {}", err);
                return;
            }

            match copy_bidirectional_with_sizes(&mut client_socket, &mut service_socket, a_b_buffer as usize, b_a_buffer as usize).await {
                Ok(_) => println!("Client disconnected."),
                Err(err) => eprintln!("Data copy error: {}", err),
            }
        }
        Err(err) => eprintln!("Failed to connect to service: {}", err),
    }
}
