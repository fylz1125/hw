use mio;

use crate::{
    server::{
        server::HWServer,
        coretypes::ClientId,
        actions::{Action, Action::*}
    },
    protocol::messages::{
        HWProtocolMessage, HWServerMessage::*
    },
    utils::is_name_illegal
};

pub fn handle(server: & mut HWServer, client_id: ClientId, message: HWProtocolMessage) {
    match message {
        HWProtocolMessage::Nick(nick) => {
            let client = &mut server.clients[client_id];
            debug!("{} {}", nick, is_name_illegal(&nick));
            let actions = if client.room_id != None {
                unreachable!()
            }
            else if !client.nick.is_empty() {
                vec![ProtocolError("Nickname already provided.".to_string())]
            }
            else if     is_name_illegal(&nick) {
                vec![ByeClient("Illegal nickname! Nicknames must be between 1-40 characters long, must not have a trailing or leading space and must not have any of these characters: $()*+?[]^{|}".to_string())]
            }
            else {
                client.nick = nick.clone();
                vec![Nick(nick).send_self().action(),
                     CheckRegistered]
            };

            server.react(client_id, actions);
        }
        HWProtocolMessage::Proto(proto) => {
            let client = &mut server.clients[client_id];
            let actions = if client.protocol_number != 0 {
                vec![ProtocolError("Protocol already known.".to_string())]
            }
            else if proto == 0 {
                vec![ProtocolError("Bad number.".to_string())]
            }
            else {
                client.protocol_number = proto;
                vec![Proto(proto).send_self().action(),
                     CheckRegistered]
            };
            server.react(client_id, actions);
        }
        #[cfg(feature = "official-server")]
        HWProtocolMessage::Checker(protocol, nick, password) => {
            let c = &mut server.clients[client_id];
            c.nick = nick;
            c.web_password = password;
            c.set_is_checker(true);
        }
        _ => warn!("Incorrect command in logging-in state"),
    }
}
