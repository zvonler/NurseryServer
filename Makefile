
PROFILE = funhouse

PROJECT := $(shell basename $(CURDIR))
SOURCES := $(wildcard $(PROJECT)/*)
RESOURCES_DIR = $(PROJECT)/resources
RESOURCES := $(wildcard $(RESOURCES_DIR)/*)

BUILD_DIR = build
BINFILE = $(BUILD_DIR)/$(PROJECT).ino.bin
FS_IMAGE = $(BUILD_DIR)/resources.lfs
MKLITTLEFS = $(BUILD_DIR)/mklittlefs/mklittlefs

PYTHON = /opt/homebrew/bin/python3.11
VENV_DIR = $(BUILD_DIR)/venv

ARDUINO_CLI = arduino-cli --profile $(PROFILE)
FQBN := $(shell $(ARDUINO_CLI) compile --show-properties $(PROJECT) | grep build.fqbn | cut -d = -f 2)
CHIP := $(shell echo $(FQBN) | sed -e 's/.*_//')
BAUD = 921600
CORE = $(HOME)/Library/Arduino15/packages/esp32/hardware/esp32/2.0.11/tools/partitions/boot_app0.bin
UPLOAD_FQBN = esp32:esp32:esp32s2

.PHONY: all clean compile dump properties upload

all: $(BINFILE) $(FS_IMAGE)

$(BINFILE): $(SOURCES)
	$(ARDUINO_CLI) compile --board-options PartitionScheme=noota_3g --output-dir $(BUILD_DIR) $(PROJECT)

clean:
	@rm -rf $(BUILD_DIR)

compile: $(BINFILE)

dump: $(CFG_FILE)
	arduino-cli config dump

$(FS_IMAGE): venv $(RESOURCES) $(MKLITTLEFS)
	$(MKLITTLEFS) -c $(RESOURCES_DIR) -s 3014656 $(FS_IMAGE)

$(MKLITTLEFS):
	@if [ ! -d $(BUILD_DIR)/mklittlefs ]; then \
	git clone git@github.com:earlephilhower/mklittlefs.git $(BUILD_DIR)/mklittlefs; \
	cd $(BUILD_DIR)/mklittlefs && git submodule update --init && make dist; \
	fi

properties:
	$(ARDUINO_CLI) compile --show-properties $(PROJECT)

upload: $(BINFILE) $(FS_IMAGE)
	$(eval PORT=$(shell arduino-cli board list | grep $(FQBN) | cut -d ' ' -f 1))
	@if [ -n "$(PORT)" ]; then \
		echo "Resetting $(PORT) to trigger bootloader"; \
		screen -dmS reset_port $(PORT) 1200 -X C-a \\\\; \
		sleep 5; \
		UPLOAD_PORT=$$(arduino-cli board list | grep $(UPLOAD_FQBN) | cut -d ' ' -f 1); \
		if [ -n "$${UPLOAD_PORT}" ]; then \
			echo "Uploading to $${UPLOAD_PORT}"; \
			$(VENV_DIR)/bin/esptool.py --chip $(CHIP) --port $${UPLOAD_PORT} --baud $(BAUD) \
				--before default_reset --after no_reset write_flash -z \
				--flash_mode dio --flash_freq 80m --flash_size 4MB \
				0x1000 "$(BUILD_DIR)/$(PROJECT).ino.bootloader.bin" \
				0x8000 "$(BUILD_DIR)/$(PROJECT).ino.partitions.bin" \
				0xe000 "$(CORE)" \
				0x10000 "$(BUILD_DIR)/$(PROJECT).ino.bin" \
				0x110000 "$(FS_IMAGE)"; \
			else \
			echo "Error: No bootloader matching $(UPLOAD_FQBN)"; \
			fi; \
		else \
		echo "Error: No board attached matching $(FQBN)"; \
		exit 1; \
	fi

venv: venv_reqs.txt
	$(PYTHON) -m venv $(VENV_DIR)
	$(VENV_DIR)/bin/pip install -r venv_reqs.txt
