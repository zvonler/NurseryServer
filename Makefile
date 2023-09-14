
BOARD = esp32:esp32:adafruit_funhouse_esp32s2
PROJECT = $(shell basename $(CURDIR))
SOURCES = $(wildcard $(PROJECT)/*)

compile: $(SOURCES)
	arduino-cli compile --fqbn "$(BOARD)" $(PROJECT)
