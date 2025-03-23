use std::io::{Error, ErrorKind, Read, Write};
use std::net::{SocketAddr, UdpSocket, TcpListener, TcpStream};
use std::time::{SystemTime, UNIX_EPOCH};
use chacha20::ChaCha20;
use chacha20::cipher::{KeyIvInit, StreamCipher};
use rand::TryRngCore;
use rand::rngs::OsRng;

const BUFFER_SIZE: usize = 1024;

pub trait MessageProcessor {
    fn process_message(&self, message: &Vec<u8>) -> Vec<u8>;
}

struct NetworkConnection {
    src: SocketAddr,
    stream: Option<TcpStream>,
    data: Vec<u8>
}

trait NetworkServer {
    fn receive(&mut self) -> Result<NetworkConnection, Error>;
    fn send(&self, connection: NetworkConnection) -> Result<(), Error>;
}

struct UdpServer {
    socket: UdpSocket
}

struct TcpServer {
    listener: TcpListener
}

pub struct BaseServer {
    network_server: Box<dyn NetworkServer>,
    message_processor: Box<dyn MessageProcessor>,
    key: [u8; 32]
}

impl UdpServer {
    fn new(port_number: u16) -> Result<UdpServer, Error> {
        let socket = UdpSocket::bind(SocketAddr::from(([0, 0, 0, 0], port_number)))?;
        Ok(UdpServer{socket})
    }
}

impl NetworkServer for UdpServer {
    fn receive(&mut self) -> Result<NetworkConnection, Error> {
        let mut buffer = [0u8; BUFFER_SIZE];
        let (n, src) = self.socket.recv_from(& mut buffer)?;
        Ok(NetworkConnection{src, stream: None, data:buffer[0..n].to_vec()})
    }

    fn send(&self, connection: NetworkConnection) -> Result<(), Error> {
        self.socket.send_to(&connection.data, connection.src)?;
        Ok(())
    }
}

impl TcpServer {
    fn new(port_number: u16) -> Result<TcpServer, Error> {
        let listener = TcpListener::bind(SocketAddr::from(([0, 0, 0, 0], port_number)))?;
        Ok(TcpServer{listener})
    }
}

impl NetworkServer for TcpServer {
    fn receive(&mut self) -> Result<NetworkConnection, Error> {
        let (mut stream, src) =  self.listener.accept()?;
        let mut buffer = [0u8; BUFFER_SIZE];
        let n = stream.read(&mut buffer)?;
        Ok(NetworkConnection{src, stream: Some(stream), data:buffer[0..n].to_vec()})
    }

    fn send(&self, connection: NetworkConnection) -> Result<(), Error> {
        connection.stream.unwrap().write_all(&connection.data)
    }
}

fn build_network_server(udp: bool, port_number: u16) -> Result<Box<dyn NetworkServer>, Error> {
    if udp {
        Ok(Box::new(UdpServer::new(port_number)?))
    } else {
        Ok(Box::new(TcpServer::new(port_number)?))
    }
}

impl BaseServer {
    pub fn new(udp: bool, port_number: u16, message_processor: Box<dyn MessageProcessor>, key: [u8; 32])
        -> Result<BaseServer, Error> {
        Ok(BaseServer { network_server: build_network_server(udp, port_number)?, message_processor, key })
    }

    pub fn start(&mut self) -> Result<(), Error> {
        loop {
            let mut connection = self.network_server.receive()?;
            let response = self.message_processor.process_message(&connection.data);
            if !response.is_empty() {
                let encrypted = self.encrypt(response)?;
                connection.data = encrypted;
                self.network_server.send(connection)?;
            }
        }
    }

    fn encrypt(&self, data: Vec<u8>) -> Result<Vec<u8>, Error> {
        let iv = self.build_iv()?;
        let mut cipher = ChaCha20::new((&self.key).into(), (&iv).into());
        let bytes = data.as_slice();
        let mut out_vec = vec![0u8; bytes.len()];
        let out_bytes = out_vec.as_mut_slice();
        cipher.apply_keystream_b2b(bytes, out_bytes)
            .map_err(|e| Error::new(ErrorKind::Other, e.to_string()))?;
        let mut result = Vec::from(iv);
        result.append(&mut out_vec);
        Ok(result)
    }

    fn build_iv(&self) -> Result<[u8; 12], Error> {
        let mut random_part = [0u8; 4];
        OsRng.try_fill_bytes(&mut random_part)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        let mut iv = [0u8; 12];
        let now = SystemTime::now().duration_since(UNIX_EPOCH)
            .map_err(|e| Error::new(ErrorKind::Other, e))?;
        iv[0..4].copy_from_slice(&random_part);
        let mut time_bytes = now.as_secs().to_le_bytes();
        time_bytes[0] ^= random_part[0];
        time_bytes[1] ^= random_part[1];
        time_bytes[2] ^= random_part[2];
        time_bytes[3] ^= random_part[3];
        time_bytes[4] ^= random_part[0];
        time_bytes[5] ^= random_part[1];
        time_bytes[6] ^= random_part[2];
        time_bytes[7] ^= random_part[3];
        iv[4..12].copy_from_slice(&time_bytes);
        Ok(iv)
    }
}
