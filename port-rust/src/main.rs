#![allow(unused)]
use libc::protoent;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{copy_bidirectional, AsyncReadExt, AsyncWriteExt};

static HTTP_PORT:u16 = 6970;

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
enum Protocol {
    Http(bool, u16),
    Https(bool, u16),
    Ssh(bool, u16),
    Mqtt(bool, u16),
    Unknown(bool, u16),
}

async fn handle_connection(mut client_socket:TcpStream, protocol:Protocol){
    match TcpStream::connect(format!("127.0.0.1:{}",HTTP_PORT).as_str()).await {
        Ok(mut service_socket) => {
            println!("Connected to service on port {}", HTTP_PORT);
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

fn find_protocol(buffer:Vec<u8>) -> Protocol{
    println!("As string: {}", String::from_utf8_lossy(&buffer));
    return Protocol::Http(true, HTTP_PORT);
}

#[tokio::main]
async fn main() {
    let mut client_listener = TcpListener::bind("0.0.0.0:8080").await.unwrap();

    loop {
        match client_listener.accept().await {
            Ok((mut client_socket, _addr)) => {
                //main code
                let mut buffer = vec![0; 4096];
                match client_socket.read(&mut buffer).await {
                    Ok(_current_message) => {},
                    Err(err) => {
                        eprintln!("Failed to accept connection: {}", err);
                    }
                }
                let protocol = find_protocol(buffer);
                tokio::spawn(async move {
                    handle_connection(client_socket, protocol).await;
                });
            }
            Err(e) => {
                eprintln!("Failed to accept connection: {}", e);
            }
        }
    }
}
