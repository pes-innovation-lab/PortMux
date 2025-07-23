use serde_yml::Value;
use regex::Regex;
use std::{fs};

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Protocol {
    pub name: &'static str,
    pub port: u16,
    pub priority: String
}

static TLS_MAJOR: u8 = 0x03;
static TLS_HANDSHAKE_RECORD: u8 = 0x16;

pub fn parse_sni(buffer: &[u8]) -> Option<String> {
    let mut pos = 0;
    
    // Check minimum length and TLS record type
    if buffer.len() < 5 || buffer[0] != TLS_HANDSHAKE_RECORD || buffer[1] != TLS_MAJOR {
        return None;
    }
    pos += 5; // Skip record header

    // Check handshake type
    if pos + 1 >= buffer.len() || buffer[pos] != 0x01 {
        return None;
    }

    pos += 4; // Handshake type + length
    pos += 2; // Version
    pos += 32; // Random

    // Check session ID length
    if pos >= buffer.len() {
        return None;
    }

    let session_id_len = buffer[pos] as usize;
    pos += 1 + session_id_len;

    // Check cipher suites length
    if pos + 2 > buffer.len() {
        return None;
    }

    let cipher_suites_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
    pos += 2 + cipher_suites_len;

    // Check compression methods length
    if pos >= buffer.len() {
        return None;
    }

    let compression_methods_len = buffer[pos] as usize;
    pos += 1 + compression_methods_len;

    // Check extensions length
    if pos + 2 > buffer.len() {
        return None;
    }

    let extensions_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
    pos += 2;

    let end = pos + extensions_len;

    // Parse extensions
    while pos + 4 <= end && pos + 4 <= buffer.len() {
        let ext_type = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]);
        let ext_len = u16::from_be_bytes([buffer[pos + 2], buffer[pos + 3]]) as usize;
        pos += 4;

        // SNI extension (type 0x00)
        if ext_type == 0x00 && pos + 2 <= buffer.len() {
            let sni_list_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
            pos += 2;

            if pos + sni_list_len <= buffer.len() && pos + 3 <= buffer.len() {
                let name_type = buffer[pos];
                let name_len = u16::from_be_bytes([buffer[pos + 1], buffer[pos + 2]]) as usize;
                pos += 3;

                if name_type == 0 && pos + name_len <= buffer.len() {
                    return Some(String::from_utf8_lossy(&buffer[pos..pos + name_len]).to_string());
                }
            }
        }

        pos += ext_len;
    }

    None
}

pub fn find_protocol(buffer: &[u8]) -> Option<Protocol> {
    let config: Value = serde_yml::from_str(&fs::read_to_string("config.yaml").unwrap()).unwrap();
    let message = String::from_utf8_lossy(&buffer);


    if buffer.starts_with(b"GET ") || buffer.starts_with(b"POST ") || buffer.windows(4).any(|w| w == b"HTTP") {
        if let Some(http) = config["HTTP"].as_mapping() {
            for (key, value) in http {
                if message.contains(key.as_str().unwrap()){
                    return Some(Protocol { name: "HTTP", port: value["port"].as_u64().unwrap() as u16, priority: value["priority"].as_str().unwrap().to_string()})
                }
            }
            return Some(Protocol { name: "HTTP", port: http["default"]["port"].as_u64().unwrap() as u16, priority: http["default"]["priority"].as_str().unwrap().to_string()});// defaulting to prt 80 if nothing matches
        }
    }
    if buffer.len() >= 3 && buffer[0] == 0x16 && buffer[1] == 0x03 && buffer[2] <= 0x03 {
        if let Some(service) = parse_sni(buffer) {
            if let Some(https) = config["HTTPS"].as_mapping() {
                for (key, value) in https {
                    if service.contains(key.as_str().unwrap()){
                        return Some(Protocol { name: "HTTPS", port: value["port"].as_u64().unwrap() as u16, priority: value["priority"].as_str().unwrap().to_string()})
                    }
                }
                return Some(Protocol { name: "HTTPS", port: https["default"]["port"].as_u64().unwrap() as u16, priority: https["default"]["priority"].as_str().unwrap().to_string()})
            }
        }
    }
    if buffer.windows(3).any(|w| w == b"SSH") {
        if let Some(ssh) = config["SSH"].as_mapping()
        {
            for (_,value) in ssh {
                return Some(Protocol{ name: "SSH", port:value.as_u64().unwrap() as u16, priority: "latency".to_string()}); 
            }
        }
    }

    if buffer.len() > 2 {
        // TCP mode: first 2 bytes are packet length, actual data starts at buffer[2]
        let tcp_opcode = buffer[2] >> 3;
        if (1..=7).contains(&tcp_opcode) {
            if let Some(openvpn) = config["OPENVPN"].as_mapping() {
                for (_, value) in openvpn {
                    match value["port"].as_u64() {
                        Some(port_num) => {
                            return Some(Protocol { name: "OPENVPN", port: port_num as u16 , priority: value["priority"].as_str().unwrap().to_string()});
                        }
                        None => break
                    }
                }
            } else {
                return Some(Protocol { name: "OPENVPN", port: config["OPENVPN"]["default"]["port"].as_u64().unwrap() as u16, priority : config["OPENVPN"]["default"]["priority"].as_str().unwrap().to_string() });
            }
        }
    }
    
    if let Some(userdefined) = config["USER"].as_mapping() {
        for (key, value) in userdefined {
            println!("{:?}:{:?}",key,value);
            let pattern = key.as_str().unwrap();
            let re = Regex::new(&pattern).unwrap();
            if re.is_match(&message){
                return Some(Protocol { name: "Custom", port: value["port"].as_u64().unwrap() as u16, priority: value["priority"].as_str().unwrap().to_string()})
            }
        }
        return Some(Protocol { name: "Custom", port: userdefined["default"]["port"].as_u64().unwrap() as u16, priority : userdefined["default"]["priority"].as_str().unwrap().to_string() })
    }
    
    // if buffer.len() > 0 {
    //     //UDP Mode: the opcode is in the first byte
    //     let opcode = buffer[0] >> 3;
    //     if matches!(opcode, 0x01..=0x07) {
    //         if let Some(openvpn) = config["openvpn"].as_mapping() {
    //             for (_, value) in openvpn {
    //                 match value.as_u64() {
    //                     Some(port_num) => {
    //                         return Some(Protocol { name: "OPENVPN", port: port_num as u16});
    //                     }

    //                     None => return Some(Protocol { name: "OPENVPN", port: 1194 })
    //                 }
    //             }
    //         } else {
    //             return Some(Protocol { name: "OPENVPN", port: 1194 });
    //         }
    //     }
    // }
    None
}