#![allow(unused)]
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{copy_bidirectional, AsyncReadExt, AsyncWriteExt};

static HTTP_PORT:u16 = 6970;

async fn handle_client(mut client_socket:TcpStream){
    match TcpStream::connect(format!("127.0.0.1:{}",HTTP_PORT).as_str()).await {
        Ok(mut service_socket) => {
            println!("Connected to service on port {}", HTTP_PORT);
            match copy_bidirectional(&mut client_socket, &mut service_socket).await {
                Ok(_) => {},
                Err(err) => {
                    eprintln!("Failed to copy data from client_socket to service : {}", err)
                }
            }
        },
        Err(err) => {
            eprintln!("Failed to connect to service: {}", err);
        }
    }
}

#[tokio::main]
async fn main() {
    let mut client_listener = TcpListener::bind("0.0.0.0:8080").await.unwrap();

    loop {
        match client_listener.accept().await {
            Ok((client_socket, _addr)) => {
                tokio::spawn(async move {
                    handle_client(client_socket).await;
                });
            }
            Err(e) => {
                eprintln!("Failed to accept connection: {}", e);
            }
        }
    }
    
}
