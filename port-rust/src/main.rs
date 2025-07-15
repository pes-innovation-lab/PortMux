#![allow(unused)]
use libc::protoent;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{copy_bidirectional, AsyncReadExt, AsyncWriteExt};

static HTTP_PORT:u16 = 6970;
static HTTPS_PORT:u16 = 443;
static SSH_PORT:u16 = 22;

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
struct Protocol {
    name: &'static str,
    port: u16,
}


async fn handle_connection(mut client_socket:TcpStream, protocol:Protocol, buffer:Vec<u8>){


    match TcpStream::connect(format!("127.0.0.1:{}",protocol.port).as_str()).await {
        Ok(mut service_socket) => {
            println!("Connected to service on port {}", protocol.port);
            
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

fn find_protocol(buffer: &Vec<u8>) -> Option<Protocol> {
    let message = String::from_utf8_lossy(buffer);
    println!("{}",message);

    if message.contains("HTTP") {
        return Some(Protocol { name: "HTTP", port: 6970 });
    }
    if message.contains("HTTPS") {
        return Some(Protocol { name: "HTTPS", port: 443 });
    }
    if message.contains("SSH") {
        return Some(Protocol { name: "SSH", port: 22 });
    }
    None
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
                
                let protocol = find_protocol(&buffer).unwrap();
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
