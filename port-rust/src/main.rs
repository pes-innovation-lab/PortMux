#![allow(unused)]
use libc::protoent;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{copy_bidirectional, AsyncReadExt, AsyncWriteExt};

static HTTP_PORT:u16 = 6970;
static HTTPS_PORT:u16 = 443;
static SSH_PORT:u16 = 22;

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
enum Protocol {
    Http(u16),
    Https(u16),
    Ssh(u16),
    Mqtt(u16),
    Unknown,
}

async fn handle_connection(mut client_socket:TcpStream, protocol:Protocol, buffer:Vec<u8>){
    let port:u16 = 80;
    match protocol {
        Protocol::Http(port) => { port; },
        Protocol::Https(port) => { port; },
        Protocol::Ssh(port) => { port; },
        _ => {},
    }
    
    match TcpStream::connect(format!("127.0.0.1:{}",port).as_str()).await {
        Ok(mut service_socket) => {
            println!("Connected to service on port {}", port);
            
            if let Err(err) = service_socket.write_all(&buffer).await {
                eprintln!("Failed to write initial buffer to service: {}", err);
                return;
            }

            match copy_bidirectional(&mut client_socket, &mut service_socket).await {
                Ok(_) => {},
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

fn find_protocol(buffer:&Vec<u8>) -> Protocol{
    let message = String::from_utf8_lossy(buffer);

    if (message.contains("HTTP")) {
        return Protocol::Http(HTTP_PORT);
    }
    if (message.contains("SSH")) {
        return Protocol::Ssh(SSH_PORT);
    }
    if (message.contains("HTTPS")){
        return Protocol::Https(HTTPS_PORT)
    }
    return Protocol::Unknown;
}

#[tokio::main]
async fn main() {
    let mut client_listener = TcpListener::bind("0.0.0.0:8080").await.unwrap();

    loop {
        match client_listener.accept().await {
            Ok((mut client_socket, _addr)) => {
                let mut buffer = vec![0; 4096];
                match client_socket.read(&mut buffer).await {
                    Ok(_current_message) => {},
                    Err(err) => {
                        eprintln!("Failed to accept connection: {}", err);
                    }
                }
                
                let protocol = find_protocol(&buffer);
                tokio::spawn(async move {
                    handle_connection(client_socket, protocol, buffer).await;
                });
            }
            Err(e) => {
                eprintln!("Failed to accept connection: {}", e);
            }
        }
    }
}

// match TcpStream::connect(format!("127.0.0.1:{}",HTTP_PORT).as_str()).await {
//         Ok(mut service_socket) => {
//             println!("Connected to service on port {}", HTTP_PORT);
            
//             if let Err(err) = service_socket.write_all(&buffer).await {
//                 eprintln!("Failed to write initial buffer to service: {}", err);
//                 return;
//             }

//             match copy_bidirectional(&mut client_socket, &mut service_socket).await {
//                 Ok(_) => {},
//                 Err(err) => {
//                     eprintln!("Failed to copy data from client_socket to service : {}", err)
//                 }
//             }
//         }
//         Err(err) => {
//             eprintln!("Failed to connect to service: {}", err);
//         }
//     }
