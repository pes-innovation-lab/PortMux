#![allow(unused)]
use std::collections::HashMap;
use tokio::io::{AsyncReadExt, AsyncWriteExt, copy_bidirectional};
use tokio::net::{TcpListener, TcpStream};
use serde_yml::{Value};
use std::fs;

static HTTP_PORT: u16 = 6970;
static HTTPS_PORT: u16 = 443;
static SSH_PORT: u16 = 22;
static TLS_MAJOR: u8 = 0x03;
static TLS_MINOR: u8 = 0x03;
static TLS_HANDSHAKE_RECORD: u8 = 0x16;

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
struct Protocol {
    name: &'static str,
    port: u16,
}

fn get_https_backend_port_for_sni(sni: &str) -> Option<u16> {
    let mut sni_map = HashMap::new();
    sni_map.insert("example.com", 8443);
    sni_map.insert("test.local", 9443);
    sni_map.get(sni).copied()
}

fn parse_sni(buffer: &[u8]) -> Option<String> {
    let mut pos = 0;
    
    // Check minimum length and TLS record type
    if buffer.len() < 5 || buffer[0] != 0x16 || buffer[1] != 0x03 {
        return None;
    }
    pos += 5; // Skip record header

    // Check handshake type
    if pos + 1 >= buffer.len() || buffer[pos] != 0x01 {
        return None;
    }

    pos += 4; // Handshake type + length
    pos += 2; // Version
    pos += 32; // Random

    // Check session ID length
    if pos >= buffer.len() {
        return None;
    }

    let session_id_len = buffer[pos] as usize;
    pos += 1 + session_id_len;

    // Check cipher suites length
    if pos + 2 > buffer.len() {
        return None;
    }

    let cipher_suites_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
    pos += 2 + cipher_suites_len;

    // Check compression methods length
    if pos >= buffer.len() {
        return None;
    }

    let compression_methods_len = buffer[pos] as usize;
    pos += 1 + compression_methods_len;

    // Check extensions length
    if pos + 2 > buffer.len() {
        return None;
    }

    let extensions_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
    pos += 2;

    let end = pos + extensions_len;

    // Parse extensions
    while pos + 4 <= end && pos + 4 <= buffer.len() {
        let ext_type = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]);
        let ext_len = u16::from_be_bytes([buffer[pos + 2], buffer[pos + 3]]) as usize;
        pos += 4;

        // SNI extension (type 0x00)
        if ext_type == 0x00 && pos + 2 <= buffer.len() {
            let sni_list_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
            pos += 2;

            if pos + sni_list_len <= buffer.len() && pos + 3 <= buffer.len() {
                let name_type = buffer[pos];
                let name_len = u16::from_be_bytes([buffer[pos + 1], buffer[pos + 2]]) as usize;
                pos += 3;

                if name_type == 0 && pos + name_len <= buffer.len() {
                    return Some(String::from_utf8_lossy(&buffer[pos..pos + name_len]).to_string());
                }
            }
        }

        pos += ext_len;
    }

    None
}

async fn handle_connection(mut client_socket: TcpStream, protocol: Protocol, buffer: Vec<u8>) {
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

fn find_protocol(buffer: &[u8]) -> Option<Protocol> {
    let config: Value = serde_yml::from_str(&fs::read_to_string("config.yaml").unwrap()).unwrap();
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
    
    // Check for HTTPS/TLS
    if buffer.len() >= 3 && buffer[0] == 0x16 && buffer[1] == 0x03 && buffer[2] <= 0x03 {
        if let Some(sni) = parse_sni(buffer) {
            println!("SNI Detected: {}", sni);
            if let Some(port) = get_https_backend_port_for_sni(&sni) {
                return Some(Protocol {
                    name: "HTTPS",
                    port,
                });
            }
        }
        // Fallback to default HTTPS port if SNI parsing fails
        return Some(Protocol {
            name: "HTTPS",
            port: HTTPS_PORT,
        });
    }
    
    // Check for SSH
    if buffer.starts_with(b"SSH-") {
        return Some(Protocol {
            name: "SSH",
            port: SSH_PORT,
        });
    }
    
    None
}

#[tokio::main]
async fn main() {
    let mut client_listener = TcpListener::bind("0.0.0.0:8080").await.unwrap();


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
                            println!("Detected protocol: {} -> port {}", protocol.name, protocol.port);
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