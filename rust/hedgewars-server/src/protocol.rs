use hedgewars_network_protocol::{
    messages::HwProtocolMessage,
    parser::{malformed_message, message},
};
use log::*;
use netbuf;
use std::io::{Read, Result};

pub struct ProtocolDecoder {
    buf: netbuf::Buf,
    is_recovering: bool,
}

impl ProtocolDecoder {
    pub fn new() -> ProtocolDecoder {
        ProtocolDecoder {
            buf: netbuf::Buf::new(),
            is_recovering: false,
        }
    }

    fn recover(&mut self) -> bool {
        self.is_recovering = match malformed_message(&self.buf[..]) {
            Ok((tail, ())) => {
                let length = tail.len();
                self.buf.consume(self.buf.len() - length);
                false
            }
            _ => {
                self.buf.consume(self.buf.len());
                true
            }
        };
        !self.is_recovering
    }

    pub fn read_from<R: Read>(&mut self, stream: &mut R) -> Result<usize> {
        let count = self.buf.read_from(stream)?;
        if count > 0 && self.is_recovering {
            self.recover();
        }
        Ok(count)
    }

    pub fn extract_messages(&mut self) -> Vec<HwProtocolMessage> {
        let mut messages = vec![];
        if !self.is_recovering {
            while !self.buf.is_empty() {
                match message(&self.buf[..]) {
                    Ok((tail, message)) => {
                        messages.push(message);
                        let length = tail.len();
                        self.buf.consume(self.buf.len() - length);
                    }
                    Err(nom::Err::Incomplete(_)) => break,
                    Err(nom::Err::Failure(e) | nom::Err::Error(e)) => {
                        debug!("Invalid message: {:?}", e);
                        if !self.recover() || self.buf.is_empty() {
                            break;
                        }
                    }
                }
            }
        }
        messages
    }
}
