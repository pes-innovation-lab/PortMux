const TLS_HANDSHAKE_RECORD: u8 = 0x16;
const TLS_MAJOR: u8 = 0x03;

pub fn parse_sni(buffer: &[u8]) -> Option<String> {
    let mut pos = 0;

    if buffer.len() < 5 || buffer[0] != TLS_HANDSHAKE_RECORD || buffer[1] != TLS_MAJOR {
        return None;
    }
    pos += 5;

    if pos + 1 >= buffer.len() || buffer[pos] != 0x01 {
        return None;
    }

    pos += 4 + 2 + 32;

    if pos >= buffer.len() {
        return None;
    }

    let session_id_len = buffer[pos] as usize;
    pos += 1 + session_id_len;

    if pos + 2 > buffer.len() {
        return None;
    }

    let cipher_suites_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
    pos += 2 + cipher_suites_len;

    if pos >= buffer.len() {
        return None;
    }

    let compression_methods_len = buffer[pos] as usize;
    pos += 1 + compression_methods_len;

    if pos + 2 > buffer.len() {
        return None;
    }

    let extensions_len = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]) as usize;
    pos += 2;

    let end = pos + extensions_len;

    while pos + 4 <= end && pos + 4 <= buffer.len() {
        let ext_type = u16::from_be_bytes([buffer[pos], buffer[pos + 1]]);
        let ext_len = u16::from_be_bytes([buffer[pos + 2], buffer[pos + 3]]) as usize;
        pos += 4;

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
