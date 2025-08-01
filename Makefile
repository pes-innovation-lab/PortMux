BINARY=portmux
INSTALL_PATH=/usr/local/bin
SERVICE_NAME=portmux.service
SERVICE_INSTALL_PATH=/etc/systemd/system
CONFIG_DIR=/etc/portmux
SERVICE_USER=portmux
YAML_CONFIG=config.yaml
PYTHON_SCRIPT=script.py


.PHONY: all install clean uninstall

all:
	cargo build --release

install: all

	# Install binary
	sudo install -Dm755 target/release/$(BINARY) $(INSTALL_PATH)/$(BINARY)

	# Install systemd service
	sudo install -Dm644 $(SERVICE_NAME) $(SERVICE_INSTALL_PATH)/$(SERVICE_NAME)

	# Creating portmux user 
	@if ! id -u $(SERVICE_USER) >/dev/null 2>&1; then \
		echo "Creating portmux user..."; \
		sudo useradd --system --no-create-home --shell /usr/sbin/nologin --comment "PORTMUX Daemon" $(SERVICE_USER); \
	else \
		echo "portmux user already exists."; \
	fi

	# Create config dir and secure permissions
	sudo mkdir -p $(CONFIG_DIR)
	sudo chown root:$(SERVICE_USER) $(CONFIG_DIR)
	sudo chmod 755 $(CONFIG_DIR)

	# Install config files as world-readable
	sudo install -m644 $(YAML_CONFIG) $(CONFIG_DIR)/$(YAML_CONFIG)
	sudo install -m644 $(PYTHON_SCRIPT) $(CONFIG_DIR)/$(PYTHON_SCRIPT)


	# Reload systemd and start service
	sudo systemctl daemon-reexec
	sudo systemctl daemon-reload
	sudo systemctl enable $(SERVICE_NAME)
	sudo systemctl restart $(SERVICE_NAME)

clean:
	cargo clean

uninstall:
	sudo systemctl stop $(SERVICE_NAME)
	sudo systemctl disable $(SERVICE_NAME)
	sudo rm -f $(SERVICE_INSTALL_PATH)/$(SERVICE_NAME)
	sudo rm -f $(INSTALL_PATH)/$(BINARY)

