def analyse(buffer):
    # buffer is of u8 bytes type.
    print(f"Analyzing buffer: {buffer}")
    # Simulate some analysis and return a port number
    if(buffer.startswith(b"PING")):
        return 6969
    elif(buffer.startswith(b"DATA")):
        return 6970

    else:
        return None
