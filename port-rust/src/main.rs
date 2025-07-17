#![allow(unused)]
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{copy_bidirectional, AsyncReadExt, AsyncWriteExt};
use serde_yaml::{Value};
use std::fs;

static HTTP_PORT:u16 = 6970;
static HTTPS_PORT:u16 = 443;
static SSH_PORT:u16 = 22;
static TLS_MAJOR:u8 = 0x03;
static TLS_MINOR:u8 = 0x03;
static TLS_HANDSHAKE_RECORD:u8 = 0x16;

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
struct Protocol {
    name: &'static str,
    port: u16,
} // instead of enum we use structs, its just easier, name is used for string matching with ymal/json in the future -> irrelevent for now


async fn handle_connection(mut client_socket:TcpStream, protocol:Protocol, buffer:Vec<u8>){
    match TcpStream::connect(format!("127.0.0.1:{}",protocol.port)).await { // match used for error checking -> contingency if connection fails 
        Ok(mut service_socket) => {
            println!("Connected to service on port {}", protocol.port);
            
            if let Err(err) = service_socket.write_all(&buffer).await { // writing the intercepted first message manually (with error checks)
                eprintln!("Failed to write initial buffer to service: {}", err);
                return;
            }

            match copy_bidirectional(&mut client_socket, &mut service_socket).await {  
                Ok(_) => {}, // taking client socket and service socket connection and letting connnection sit indefinitely, until disconnection
                Err(err) => {
                    eprintln!("Failed to copy data from client_socket to service : {}", err)
                }
            }
        }
        Err(err) => {
            eprintln!("Failed to connect to service: {}", err);
        }
    }
}

fn find_protocol(buffer: &[u8]) -> Option<Protocol> {
    let config: Value = serde_yaml::from_str(&fs::read_to_string("config.yaml").unwrap()).unwrap();
    let message = String::from_utf8_lossy(&buffer);

    if buffer.starts_with(b"GET ") || buffer.starts_with(b"POST ") || buffer.windows(4).any(|w| w == b"HTTP") {
        if let Some(http) = config["http"].as_mapping() {
            for (key, value) in http {
                if message.contains(key.as_str().unwrap()){
                    return Some(Protocol { name: "HTTP", port: value.as_u64().unwrap() as u16})
                }
            }
        }
        return Some(Protocol { name: "HTTP", port: 80 }); // defaulting to prt 80 if nothing matches
    }
    if buffer.len() >= 3 && buffer[0] == 0x16 && buffer[1] == 0x03 && buffer[2] <= 0x03 {
        return Some(Protocol { name: "HTTPS", port: 443 });
    }
    if buffer.windows(3).any(|w| w == b"SSH") {
        return Some(Protocol { name: "SSH", port: 22 });
    }
    if buffer.len() >= 1 {
        // OpenVPN packets start with specific opcodes, e.g., control channel or data channel
        // This checks for a control channel packet (could be P_CONTROL or P_ACK)
        if buffer[0] == 0x01 || buffer[0] == 0x02 || buffer[0] == 0x03 {
            return Some(Protocol { name: "OpenVPN", port: 1194 });
        }
    }
    None
}

#[tokio::main]
async fn main() {
    let mut client_listener = TcpListener::bind("0.0.0.0:8080").await.unwrap();


    loop {
        match client_listener.accept().await {
            Ok((mut client_socket, _addr)) => {
                let mut buffer = vec![0; 4096]; // 4kb buffer, subject to change depending on use case and protocol support
                let n = match client_socket.read(&mut buffer).await {
                    Ok(n) if n > 0 => n, // puttint the size of the buffer in n
                    Ok(_) => {
                        continue;
                    },
                    Err(_) => {
                        eprintln!("Failed to read from client or empty buffer.");
                        return;
                    }
                };
            
                let buffer = buffer[..n].to_vec(); // reducing the size of the vec from 4kb to actual vector size
                let protocol = find_protocol(&buffer).unwrap(); // getting the struct with the right port
                tokio::spawn(async move {
                    handle_connection(client_socket, protocol, buffer).await; // spawning a async block for this connection
                });
            }
            Err(e) => {
                eprintln!("Failed to accept connection: {}", e);
            }
        }
    }
}

