use crate::protocol::types::Protocol;
use crate::protocol::sni::parse_sni;
use crate::protocol::custom_script::custom_script;
use serde_yml::Value;

pub fn find_protocol(buffer: &[u8], config: &Value) -> Option<Protocol> {
    let msg = String::from_utf8_lossy(buffer);

    // HTTP
    if buffer.starts_with(b"GET ") || buffer.starts_with(b"POST ") || buffer.windows(4).any(|w| w == b"HTTP") {
        if let Some(http) = config.get("HTTP")?.as_mapping() {
            for (key, val) in http {
                if msg.contains(key.as_str().unwrap()) {
                    return Some(Protocol {
                        name: "HTTP",
                        port: val["port"].as_u64().unwrap() as u16,
                        priority: val["priority"].as_str().unwrap().to_string(),
                    });
                }
            }
            return Some(Protocol {
                name: "HTTP",
                port: http["default"]["port"].as_u64().unwrap() as u16,
                priority: http["default"]["priority"].as_str().unwrap().to_string(),
            });
        }
    }

    // HTTPS (TLS handshake)
    if buffer.len() >= 3 && buffer[0] == 0x16 && buffer[1] == 0x03 && buffer[2] <= 0x03 {
        if let Some(host) = parse_sni(buffer) {
            if let Some(https) = config.get("HTTPS")?.as_mapping() {
                for (key, val) in https {
                    if host.contains(key.as_str().unwrap()) {
                        return Some(Protocol {
                            name: "HTTPS",
                            port: val["port"].as_u64().unwrap() as u16,
                            priority: val["priority"].as_str().unwrap().to_string(),
                        });
                    }
                }
                return Some(Protocol {
                    name: "HTTPS",
                    port: https["default"]["port"].as_u64().unwrap() as u16,
                    priority: https["default"]["priority"].as_str().unwrap().to_string(),
                });
            }
        }
    }

    // SSH
    if buffer.windows(3).any(|w| w == b"SSH") {
        if let Some(ssh) = config.get("SSH")?.as_mapping() {
            for (_, val) in ssh {
                return Some(Protocol {
                    name: "SSH",
                    port: val.as_u64().unwrap() as u16,
                    priority: "latency".to_string(),
                });
            }
        }
    }

    // OpenVPN TCP
    if buffer.len() > 2 {
        let opcode = buffer[2] >> 3;
        if (1..=7).contains(&opcode) {
            if let Some(openvpn) = config.get("OPENVPN")?.as_mapping() {
                if let Some(val) = openvpn.get("tcp") {
                    return Some(Protocol {
                        name: "OPENVPN",
                        port: val["port"].as_u64().unwrap() as u16,
                        priority: val["priority"].as_str().unwrap().to_string(),
                    });
                }
            }
        }
    }

    // OpenVPN UDP
    if !buffer.is_empty() {
        let opcode = buffer[0] >> 3;
        if (1..=7).contains(&opcode) {
            if let Some(openvpn) = config.get("OPENVPN")?.as_mapping() {
                if let Some(val) = openvpn.get("udp") {
                    return Some(Protocol {
                        name: "OPENVPN",
                        port: val["port"].as_u64().unwrap() as u16,
                        priority: val["priority"].as_str().unwrap().to_string(),
                    });
                }
            }
        }
    }

    // Custom script fallback
    if let Ok(port) = custom_script(buffer) {
        return Some(Protocol {
            name: "Custom",
            port: port as u16,
            priority: "auto".to_string(),
        });
    }

    None
}
