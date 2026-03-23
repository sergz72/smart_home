use std::io::Error;
use std::net::{ToSocketAddrs, UdpSocket};
use std::time::Duration;

const BUFFER_SIZE: usize = 65507;

pub fn udp_send(host_name: &String, data: &Vec<u8>, read_timeout: Duration)  -> Result<Vec<u8>, Error> {
    let addr = host_name.to_socket_addrs()?.next().unwrap();
    let socket = UdpSocket::bind("[::]:0")?;
    socket.set_read_timeout(Some(read_timeout))?;
    socket.send_to(data.as_slice(), addr)?;
    let mut result_bytes = [0; BUFFER_SIZE];
    let (amt, _src) = socket.recv_from(&mut result_bytes)?;
    Ok(result_bytes[0..amt].to_vec())
}