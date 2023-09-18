
# Smart Nursery Server

Target: [Adafruit FunHouse](https://www.adafruit.com/product/4985)

The buttons on the FunHouse map to:
 - Top - increase main lights brightness
 - Middle - toggle timeout display on LED ring
 - Bottom - decrease main lights brightness

The server advertises itself at `http://{hostname}.local`.

Endpoints:
 - `/` - General status page with buttons to perform actions
 - `/brighter` - Makes lights brighter
 - `/dimmer` - Makes lights dimmer
 - `/off` - Turns lights off
 - `/wake` - Runs a wake cycle that brings the lights up slowly
 - `/status` - Returns sensor and system information as JSON
 - `/timeout` - Toggles timeout LED ring function

Status page includes:
 - Current time
 - Last time door opened / closed
 - Last time lights on / off
 - Last time motion sensed
 - Temperature
 - Humidity
 - Timeout status

The FunHouse A0 and A1 connections control the LED strips through MOSFETs.

The FunHouse A2 connection is used to power and control the LED ring.

The FunHouse I2C connection is used to talk to an MCP23008 to read the RF remote receiver signals.

![Status page example](doc/status_page_example.png?raw=true "Status Page")

![Circuit diagram](doc/circuit.png?raw=true "Circuit Diagram")
