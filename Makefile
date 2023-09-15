
PROFILE = funhouse

PROJECT := $(shell basename $(CURDIR))
SOURCES := $(wildcard $(PROJECT)/*)
BUILD_DIR = build
BINFILE = $(BUILD_DIR)/$(PROJECT).ino.bin

ARDUINO_CLI = arduino-cli --profile $(PROFILE)
FQBN := $(shell $(ARDUINO_CLI) compile --show-properties $(PROJECT) | grep build.fqbn | cut -d = -f 2)

$(BINFILE): $(SOURCES)
	$(ARDUINO_CLI) --output-dir $(BUILD_DIR) compile $(PROJECT)

dump: $(CFG_FILE)
	$(ARDUINO_CLI) config dump

clean:
	@rm -rf $(BUILD_DIR)

upload: $(BINFILE)
	$(eval PORT=$(shell arduino-cli board list | grep $(FQBN) | cut -d ' ' -f 1))
	@if [ -n "$(PORT)" ]; then \
	$(ARDUINO_CLI) upload --port $(PORT) $(PROJECT); \
	else \
	echo "Error: No board attached matching $(FQBN)"; \
	fi
