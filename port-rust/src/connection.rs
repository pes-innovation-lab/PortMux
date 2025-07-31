use crate::protocol::Protocol;
use anyhow::Context;
use tokio::io::{AsyncWriteExt, copy_bidirectional_with_sizes};
use tokio::net::TcpStream;

pub async fn handle_connection(
    mut client_socket: TcpStream,
    protocol: Protocol,
    buffer: Vec<u8>,
) -> Result<(), anyhow::Error> {
    let (a_b_buffer, b_a_buffer) = match protocol.priority.as_str() {
        "latency" => (1024, 1024),
        "throughput" => (32768, 32768),
        _ => (8192, 8192),
    };

    let mut service_socket = TcpStream::connect(format!("127.0.0.1:{}", protocol.port))
        .await
        .context("Failed to connect to service")?;

    service_socket
        .write_all(&buffer)
        .await
        .context("Failed to write to service")?;

    copy_bidirectional_with_sizes(
        &mut client_socket,
        &mut service_socket,
        a_b_buffer,
        b_a_buffer,
    )
    .await
    .context("Data copy error")?;

    Ok(())
}
