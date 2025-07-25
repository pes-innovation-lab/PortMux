use serde_yml::Value;
use pyo3::prelude::*;
use pyo3::types::PyModule;
use std::path::Path;
use std::ffi::CString;
use pyo3::types::PyBytes;

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Protocol {
    pub name: &'static str,
    pub port: u16,
    pub priority: String,
}

static TLS_MAJOR: u8 = 0x03;
static TLS_HANDSHAKE_RECORD: u8 = 0x16;

//all file reading related functions should be in a util file

fn custom_script(buffer: &[u8]) -> Result<u32, ()> {
    if !Path::new("script.py").exists() { //path is hardcoded
        eprintln!("Script file not found in the current directory!");
        return Err(());
    }

    // GIL = Global Interpreter Lock
    Python::with_gil(|py| {
        // Read Python script file
        let script_content = match std::fs::read_to_string("script.py") { //path is hardcoded
            Ok(content) => content,
            Err(e) => {
                eprintln!("Failed to read script: {}", e);
                return Err(());
            }
        };

        let filename = CString::new("script.py").unwrap();  //hardcoded
        let module_name = CString::new("script").unwrap();  //hardcoded
        let code = CString::new(script_content).unwrap();

        // Compile the python script
        let module = match PyModule::from_code(py, &code, &filename, &module_name) {
            Ok(m) => m,
            Err(e) => {
                eprintln!("Failed to compile Python script: {}", e);
                return Err(());
            }
        };

        // Get the function
        let analyse_func = match module.getattr("analyse") {
            Ok(f) => f,
            Err(e) => {
                eprintln!("Function 'analyse' not found: {}", e);
                return Err(());
            }
        };

        // Convert the u8 buffer to PyBytes
        let py_buffer = PyBytes::new(py, buffer);

        // Call analyse(buffer)
        let result = match analyse_func.call1((py_buffer,)) {
            Ok(r) => r,
            Err(e) => {
                eprintln!("Error calling analyse: {}", e);
                return Err(());
            }
        };

        // Fetch the port number from the result and return it.
        match result.extract::<u32>() {
            Ok(port) => Ok(port),
            Err(e) => {
                eprintln!("Failed to parse port number{}", e);
                Err(())
            }
        }
    })
}

pub fn parse_sni(buffer: &[u8]) -> Option<String> {
    let mut pos = 0;

    // Check minimum length and TLS record type
    //magic numbers bad
    if buffer.len() < 5 || buffer[0] != TLS_HANDSHAKE_RECORD || buffer[1] != TLS_MAJOR {
        return None;
    }
    pos += 5; // Skip record header //why +5? avoid magic numbers

    // Check handshake type
    if pos + 1 >= buffer.len() || buffer[pos] != 0x01 { //all these must be defined as macros or consts
        return None;
    }

    pos += 4; // Handshake type + length instead of comments and stuff, please use consts describing exactly what is going on
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
    //TOO many magic numbers guys
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

//all these functions should be in utils.

pub fn find_protocol(buffer: &[u8], config : &Value) -> Option<Protocol> {
    // let config: Value = serde_yml::from_str(&fs::read_to_string("config.yaml").unwrap()).unwrap();
    let message = String::from_utf8_lossy(&buffer);


    if buffer.starts_with(b"GET ")
        || buffer.starts_with(b"POST ")
        || buffer.windows(4).any(|w| w == b"HTTP")
    {
        if let Some(http) = config["HTTP"].as_mapping() {
            for (key, value) in http {
                if message.contains(key.as_str().unwrap()) {
                    return Some(Protocol {
                        name: "HTTP",
                        port: value["port"].as_u64().unwrap() as u16,
                        priority: value["priority"].as_str().unwrap().to_string(),
                    });
                }
            }
            return Some(Protocol { name: "HTTP", port: http["default"]["port"].as_u64().unwrap() as u16, priority: http["default"]["priority"].as_str().unwrap().to_string()});// defaulting to prt 80 if nothing matches
        }
    }
    //magic number bad
    //use switch for these
    if buffer.len() >= 3 && buffer[0] == 0x16 && buffer[1] == 0x03 && buffer[2] <= 0x03 {
        if let Some(service) = parse_sni(buffer) {
            if let Some(https) = config["HTTPS"].as_mapping() {
                for (key, value) in https {
                    if service.contains(key.as_str().unwrap()) {
                        return Some(Protocol {
                            name: "HTTPS",
                            port: value["port"].as_u64().unwrap() as u16,
                            priority: value["priority"].as_str().unwrap().to_string(),
                        });
                    }
                }
                return Some(Protocol { name: "HTTPS", port: https["default"]["port"].as_u64().unwrap() as u16, priority: https["default"]["priority"].as_str().unwrap().to_string()})
            }
        }
    }
    if buffer.windows(3).any(|w| w == b"SSH") {
        if let Some(ssh) = config["SSH"].as_mapping() {
            for (_, value) in ssh {
                return Some(Protocol {
                    name: "SSH",
                    port: value.as_u64().unwrap() as u16,
                    priority: "latency".to_string(),
                });
            }
        }
    }

    if buffer.len() > 2 {
        // TCP mode: first 2 bytes are packet length, actual data starts at buffer[2]
        let tcp_opcode = buffer[2] >> 3;
        if (1..=7).contains(&tcp_opcode) {
            if let Some(openvpn) = config["OPENVPN"].as_mapping() {
                if let Some(value) = openvpn.get("tcp") {
                    return Some(Protocol {
                        name: "OPENVPN",
                        port: value["port"].as_u64().unwrap() as u16,
                        priority: value["priority"].as_str().unwrap().to_string(),
                    });
                } else if let Some(default) = openvpn.get("default") {
                    return Some(Protocol {
                        name: "OPENVPN",
                        port: default["port"].as_u64().unwrap() as u16,
                        priority: default["priority"].as_str().unwrap().to_string(),
                    });
                }
            }
        }
    }

    if !buffer.is_empty() {
        // UDP Mode: the opcode is in the first byte
        let opcode = buffer[0] >> 3;
        if (1..=7).contains(&opcode) {
            if let Some(openvpn) = config["OPENVPN"].as_mapping() {
                if let Some(value) = openvpn.get("udp") {
                    return Some(Protocol {
                        name: "OPENVPN",
                        port: value["port"].as_u64().unwrap() as u16,
                        priority: value["priority"].as_str().unwrap().to_string(),
                    });
                } else if let Some(default) = openvpn.get("default") {
                    return Some(Protocol {
                        name: "OPENVPN",
                        port: default["port"].as_u64().unwrap() as u16,
                        priority: default["priority"].as_str().unwrap().to_string(),
                    });
                }
            }
        }
    }

    //Check for custom script
    if let Ok(port) = custom_script(buffer) {
        return Some(Protocol {
            name: "Custom",
            port: port as u16,
            priority: "auto".to_string(),
        });
    }
    None
}
