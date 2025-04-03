use std::io::{Error, Read, Write};
use std::net::{SocketAddr, UdpSocket, TcpListener, TcpStream};
use std::sync::Arc;
use std::thread;
use crate::logger::Logger;

const BUFFER_SIZE: usize = 1024;

pub trait MessageProcessor {
    fn process_message(&self, logger: &Logger, message: &Vec<u8>, time_offset: i64) -> Vec<u8>;
}

struct NetworkConnection {
    src: SocketAddr,
    stream: Option<TcpStream>,
    data: Vec<u8>,
    logger: Logger
}

trait NetworkServer {
    fn receive(&self) -> Result<NetworkConnection, Error>;
    fn send(&self, connection: NetworkConnection) -> Result<(), Error>;
}

struct UdpServer {
    socket: UdpSocket
}

struct TcpServer {
    listener: TcpListener
}

pub struct BaseServer {
    network_server: Box<dyn NetworkServer + Sync>,
    message_processor: Arc<dyn MessageProcessor + Sync + Send>,
    time_offset: i64,
    logger: Logger
}

unsafe impl Send for UdpServer {}
unsafe impl Send for TcpServer {}
unsafe impl Send for BaseServer {}

impl UdpServer {
    fn new(port_number: u16) -> Result<UdpServer, Error> {
        let socket = UdpSocket::bind(SocketAddr::from(([0, 0, 0, 0], port_number)))?;
        Ok(UdpServer{socket})
    }
}

impl NetworkServer for UdpServer {
    fn receive(&self) -> Result<NetworkConnection, Error> {
        let mut buffer = [0u8; BUFFER_SIZE];
        let (n, src) = self.socket.recv_from(& mut buffer)?;
        let logger = Logger::new("udp ".to_string() + src.to_string().as_str());
        Ok(NetworkConnection{src, stream: None, data:buffer[0..n].to_vec(), logger})
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
    fn receive(&self) -> Result<NetworkConnection, Error> {
        let (mut stream, src) =  self.listener.accept()?;
        let mut buffer = [0u8; BUFFER_SIZE];
        let n = stream.read(&mut buffer)?;
        let logger = Logger::new("tcp ".to_string() + src.to_string().as_str());
        Ok(NetworkConnection{src, stream: Some(stream), data:buffer[0..n].to_vec(), logger})
    }

    fn send(&self, connection: NetworkConnection) -> Result<(), Error> {
        connection.stream.unwrap().write_all(&connection.data)
    }
}

fn build_network_server(udp: bool, port_number: u16) -> Result<Box<dyn NetworkServer + Sync>, Error> {
    if udp {
        Ok(Box::new(UdpServer::new(port_number)?))
    } else {
        Ok(Box::new(TcpServer::new(port_number)?))
    }
}

impl BaseServer {
    pub fn new(udp: bool, port_number: u16, message_processor: Arc<dyn MessageProcessor + Sync + Send>,
               time_offset: i64, name: String) -> Result<BaseServer, Error> {
        Ok(BaseServer { network_server: build_network_server(udp, port_number)?, message_processor,
                        time_offset, logger: Logger::new(name) })
    }

    pub fn start(&'static self) {
        self.logger.info("Starting server...");
        loop {
            match self.network_server.receive() {
                Ok(connection) => {
                    thread::spawn(|| self.handle(connection));
                },
                Err(err) => self.logger.error(format!("Receive error: {}", err))
            }
        }
    }

    fn handle(&self, connection: NetworkConnection) {
        let logger = connection.logger.clone();
        logger.info("Received connection request.");
        if let Err(e) = self.handler(connection) {
            logger.error(e.to_string());
        }
        logger.info("Connection closed.");
    }

    fn handler(&self, mut connection: NetworkConnection) -> Result<(), Error> {
        let response = 
            self.message_processor.process_message(&connection.logger, &connection.data, self.time_offset);
        if !response.is_empty() {
            connection.data = response;
            self.network_server.send(connection)?;
        }
        Ok(())
    }
}
