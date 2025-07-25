static TLS_MAJOR: u8 = 0x03;
static TLS_HANDSHAKE_RECORD: u8 = 0x16;
static TLS_RECORD_HEADER_SIZE: usize = 5;
static TLS_HANDSHAKE_TYPE_CLIENT_HELLO: u8 = 0x01;
static TLS_HANDSHAKE_TYPE_SIZE: usize = 1;
static TLS_HANDSHAKE_MESSAGE_LENGTH : usize = 3;
static TLS_VERSION_SIZE: usize = 2;
static RANDOM_DATA_SIZE: usize = 32;
static SESSION_ID_LENGTH_SIZE: usize = 1;
static CIPHER_SUITES_LENGTH_SIZE: usize = 2;
static COMPRESSION_METHODS_LENGTH_SIZE: usize = 1;
static EXTENSIONS_FIELD_SIZE: usize = 2;
static EXTENSIONS_TOTAL_SIZE: usize = 4;
static SNI_EXTENSION_SIZE: usize = 2;
static SNI_LIST_LENGTH_SIZE: usize = 2;
static SNI_EXTENSION: u16 = 0x00;
static SNI_NAME_ENTRY_SIZE: usize = 3;
static SNI_NAME_TYPE: u8 = 0; // 0 = Hostname
static TLS_RECORD_TYPE_OFFSET: usize = 0;
static TLS_VERSION_OFFSET: usize = 1;
static U16_BYTE_0_OFFSET: usize = 0;
static U16_BYTE_1_OFFSET: usize = 1;
static EXT_TYPE_BYTE_0_OFFSET: usize = 0;
static EXT_TYPE_BYTE_1_OFFSET: usize = 1;
static EXT_LEN_BYTE_0_OFFSET: usize = 2;
static EXT_LEN_BYTE_1_OFFSET: usize = 3;
static SNI_NAME_TYPE_OFFSET: usize = 0;
static SNI_NAME_LEN_BYTE_0_OFFSET: usize = 1;
static SNI_NAME_LEN_BYTE_1_OFFSET: usize = 2;

pub fn parse_sni(buffer: &[u8]) -> Option<String> {
    let mut pos = 0;
    let buffer_length = buffer.len();

    // Check minimum length and TLS record type
    if buffer_length < TLS_RECORD_HEADER_SIZE 
        || buffer[TLS_RECORD_TYPE_OFFSET] != TLS_HANDSHAKE_RECORD 
        || buffer[TLS_VERSION_OFFSET] != TLS_MAJOR {
        return None;
    }
    pos += TLS_RECORD_HEADER_SIZE; // Skip record header

    // Check handshake type
    if pos + TLS_HANDSHAKE_TYPE_SIZE > buffer_length || buffer[pos] != TLS_HANDSHAKE_TYPE_CLIENT_HELLO {
        return None;
    }

    pos += TLS_HANDSHAKE_MESSAGE_LENGTH; // Length
    pos += TLS_HANDSHAKE_TYPE_SIZE; // Handshake type 
    pos += TLS_VERSION_SIZE; // Version
    pos += RANDOM_DATA_SIZE; // Random

    // Check session ID length
    if pos >= buffer_length {
        return None;
    }

    let session_id_len = buffer[pos] as usize;
    pos += SESSION_ID_LENGTH_SIZE + session_id_len;

    // Check cipher suites length
    if pos + CIPHER_SUITES_LENGTH_SIZE > buffer_length {
        return None;
    }

    let cipher_suites_len = u16::from_be_bytes([
        buffer[pos + U16_BYTE_0_OFFSET], 
        buffer[pos + U16_BYTE_1_OFFSET]
    ]) as usize;
    pos += CIPHER_SUITES_LENGTH_SIZE + cipher_suites_len;

    // Check compression methods length
    if pos >= buffer_length {
        return None;
    }

    let compression_methods_len = buffer[pos] as usize;
    pos += COMPRESSION_METHODS_LENGTH_SIZE + compression_methods_len;

    // Check extensions length
    if pos + EXTENSIONS_FIELD_SIZE > buffer_length {
        return None;
    }

    let extensions_len = u16::from_be_bytes([
        buffer[pos + U16_BYTE_0_OFFSET], 
        buffer[pos + U16_BYTE_1_OFFSET]
    ]) as usize;
    pos += EXTENSIONS_FIELD_SIZE;

    let end = pos + extensions_len;

    // Parse extensions
    while pos + EXTENSIONS_TOTAL_SIZE <= end && pos + EXTENSIONS_TOTAL_SIZE <= buffer_length {
        let ext_type = u16::from_be_bytes([
            buffer[pos + EXT_TYPE_BYTE_0_OFFSET], 
            buffer[pos + EXT_TYPE_BYTE_1_OFFSET]
        ]);
        let ext_len = u16::from_be_bytes([
            buffer[pos + EXT_LEN_BYTE_0_OFFSET], 
            buffer[pos + EXT_LEN_BYTE_1_OFFSET]
        ]) as usize;
        pos += EXTENSIONS_TOTAL_SIZE;

        // SNI extension (type 0x00)
        if ext_type == SNI_EXTENSION && pos + SNI_LIST_LENGTH_SIZE <= buffer_length {
            let sni_list_len = u16::from_be_bytes([
                buffer[pos + U16_BYTE_0_OFFSET], 
                buffer[pos + U16_BYTE_1_OFFSET]
            ]) as usize;
            pos += SNI_EXTENSION_SIZE;

            if pos + sni_list_len <= buffer_length && pos + SNI_NAME_ENTRY_SIZE <= buffer_length {
                let name_type = buffer[pos + SNI_NAME_TYPE_OFFSET];
                let name_len = u16::from_be_bytes([
                    buffer[pos + SNI_NAME_LEN_BYTE_0_OFFSET], 
                    buffer[pos + SNI_NAME_LEN_BYTE_1_OFFSET]
                ]) as usize;
                pos += SNI_NAME_ENTRY_SIZE;

                if name_type == SNI_NAME_TYPE && pos + name_len <= buffer_length {
                    return Some(String::from_utf8_lossy(&buffer[pos..pos + name_len]).to_string());
                }
            }
        }

        pos += ext_len;
    }

    None
}
