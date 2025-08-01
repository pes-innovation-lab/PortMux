# PortMux

A high-performance network port multiplexer daemon written in Rust that intelligently routes incoming connections to different backend services based on protocol detection and host-based routing.

## Features

* **Protocol Detection**: Automatically detects HTTP, HTTPS, OpenVPN, SSH,SFTP and custom protocols
* **Host-based Routing**: Route HTTP/HTTPS requests based on the Host header
* **Python Integration**: Custom protocol analysis through embedded Python scripts
* **Performance Optimization**: Configurable priority settings (latency, throughput, both)
* **Systemd Integration**: Full systemd service support with security hardening

## Architecture

PortMux acts as a reverse proxy that listens on a single port and forwards connections to appropriate backend services based on:

1. **Protocol Analysis**: Examines initial packet data to determine the protocol
2. **Host Header Inspection**: For HTTP/HTTPS, routes based on the Host header
3. **Custom Rules**: Uses Python scripts for advanced protocol detection
4. **Fallback Routing**: Default backends for unmatched connections

## Installation

### From AUR (Recommended for Arch Linux)

```bash
yay -S portmux
# or
paru -S portmux
```

### From Source

Clone the repository:

```bash
git clone https://github.com/yourusername/portmux.git
cd portmux
```

Build and install:

```bash
make install
```

### Manual Build

Build the binary:

```bash
cargo build --release
```

The binary will be available at `target/release/portmux`

## Configuration

The main configuration file is located at `/etc/portmux/config.yaml`. Here's the structure:

```yaml
PORTMUX:
  ip: 0.0.0.0
  port: 8080

HTTP:
  "example.com":
    port: 3000
    priority: latency
  "api.example.com":
    port: 3001
    priority: throughput
  "default":
    port: 80
    priority: latency

HTTPS:
  "secure.example.com":
    port: 3443
    priority: latency
  "default":
    port: 443
    priority: both

OPENVPN:
  "tcp":
    port: 443
    priority: both
  "udp":
    port: 1194
    priority: both
  "default":
    port: 1194
    priority: both

SSH:
  "default": 22
```

### Priority Settings

* `latency`: Optimized for low-latency connections
* `throughput`: Optimized for high-throughput transfers
* `both`: Balanced optimization

### Custom Protocol Detection

PortMux can use Python scripts for custom protocol analysis. Edit `/etc/portmux/script.py`:

```python
def analyse(buffer):
    """
    Analyze incoming buffer and return appropriate port number

    Args:
        buffer (bytes): Initial packet data

    Returns:
        int: Port number for routing, or None for default handling
    """
    if buffer.startswith(b"CUSTOM_PROTOCOL"):
        return 9000
    elif buffer.startswith(b"GAME_DATA"):
        return 7777
    else:
        return None
```

## Usage

### Starting the Service

```bash
systemctl enable --now portmux.service
```

Check status:

```bash
systemctl status portmux.service
```

View logs:

```bash
journalctl -u portmux.service -f
```

### Manual Execution

Run in foreground (for testing):

```bash
portmux
```

### Testing the Setup

Test HTTP routing:

```bash
curl -H "Host: example.com" http://your-server:8080
```

Test HTTPS routing:

```bash
curl -k -H "Host: secure.example.com" https://your-server:8080
```

Test SSH forwarding:

```bash
ssh user@your-server -p 8080
```

## Security Considerations

PortMux runs with several security hardening measures:

* **Unprivileged User**: Runs as dedicated `portmux` user
* **Capability-based**: Only has `CAP_NET_BIND_SERVICE` capability
* **Read-only Configuration**: Configuration files are read-only at runtime
* **System Protection**: Protected home directory and system files
* **No New Privileges**: Cannot escalate privileges

### Firewall Configuration(Optional)

Allow PortMux port:

```bash
sudo ufw allow 8080/tcp
```

Ensure backend services are not directly accessible:

```bash
sudo ufw deny 3000:3010/tcp
```

## Monitoring

### Logs

View real-time logs:

```bash
journalctl -u portmux.service -f
```

View logs from last hour:

```bash
journalctl -u portmux.service --since "1 hour ago"
```

### Health Checks

Check if service is running:

```bash
systemctl is-active portmux.service
```

Check listening ports:

```bash
sudo netstat -tulpn | grep portmux
```

## Troubleshooting

### Common Issues

1. **Permission Denied**: Ensure the service has `CAP_NET_BIND_SERVICE` capability
2. **Config Parse Error**: Validate YAML syntax with `yamllint config.yaml`
3. **Python Script Errors**: Check logs for Python integration issues
4. **Connection Refused**: Verify backend services are running and accessible

## Development

### Building from Source

Install Rust toolchain:

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

Clone and build:

```bash
git clone https://github.com/yourusername/portmux.git
cd portmux
cargo build --release
```

### Running Tests

```bash
cargo test
```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Changelog

### v0.1.0

* Initial release
* Basic protocol detection and routing
* HTTP/HTTPS host-based routing
* OpenVPN, SSH, SFTP support
* Python integration for custom protocols
* Systemd service integration
