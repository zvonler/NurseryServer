
PROFILE = funhouse

PROJECT = $(shell basename $(CURDIR))
SOURCES = $(wildcard $(PROJECT)/*)
BUILD_DIR = build

ARDUINO_CLI = arduino-cli --profile $(PROFILE)

compile: $(SOURCES)
	$(ARDUINO_CLI) --output-dir $(BUILD_DIR) compile $(PROJECT)

dump: $(CFG_FILE)
	$(ARDUINO_CLI) config dump

clean:
	@rm -rf $(BUILD_DIR)
