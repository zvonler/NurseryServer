/**
   Smart Nursery Server

   Target: Adafruit FunHouse

   The buttons on the FunHouse map to:
     Top - increase main lights brightness
     Middle - toggle timeout display on LED ring
     Bottom - decrease main lights brightness

   The server advertises itself at http://{hostname}.local
   Endpoints:
    / - General status page with buttons to perform actions
    /brighter - Makes lights brighter
    /dimmer - Makes lights dimmer
    /off - Turns lights off
    /wake - Runs a wake cycle that brings the lights up slowly

   Status page includes:
     Current time
     Last time door opened / closed
     Last time lights on / off
     Last time motion sensed
     Last time noise sensed
     Temperature
     Humidity

   The FunHouse A0 and A1 connections control the LED strips through MOSFETs.

   The FunHouse A2 connection is used to power and control the LED ring.

   The FunHouse I2C connection is used to talk to MCP23008 to read the RF remote
   receiver signals.
*/

#include "led_ring.h"
#include "led_strip_controller.h"
#include "nursery_monitor.h"
#include "nursery_web_server.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <time.h>

/* ------ File included next should define SECRET_SSID and SECRET_PASS ------- */
#include "arduino_secrets.h"
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
/* --------------------------------------------------------------------------- */

#define BG_COLOR ST77XX_BLACK

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RESET);
LEDStripController strip_controller(A0, A1);
LEDRing led_ring;
NurseryMonitor monitor(strip_controller, led_ring);
NurseryWebServer web_server(strip_controller, LittleFS, monitor);

/*---------------------------------------------------------------------------*/

bool backlight = true;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -6 * 3600;
const int daylightOffset_sec = 3600;

// Change for "release" builds
const char* hostname = "nursery-devel";

/*---------------------------------------------------------------------------*/

void update_tft_time() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    tft.setCursor(0, 60);
    tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
    tft.print("NTP: ");
    tft.setTextColor(ST77XX_RED, BG_COLOR);
    tft.println("Failed");
  } else {
    tft.setCursor(0, 60);
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.print("NTP: ");
    char timestr[128];
    strftime(timestr, 128, "%Y%m%d %H:%M", &timeinfo);
    tft.println(timestr);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  monitor.init();

strip_controller.init();

  tft.init(240, 240);  // Initialize ST7789 screen
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);  // Backlight on

  tft.fillScreen(BG_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextWrap(false);

  // check AHT!
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("AHT20: ");

  if (!monitor.aht_begin()) {
    tft.setTextColor(ST77XX_RED);
    tft.println("FAIL!");
    while (1) delay(100);
  }
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_GREEN);
  tft.print("AHT20: ");
  tft.println("OK!");

  tft.setCursor(0, 20);
  tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
  tft.print("WIFI: ");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    tft.setCursor(0, 20);
    tft.print("WIFI: ");
    const char* frames[4] = { "|", "/", "-", "\\" };
    static int frame = 0;
    tft.print(frames[frame]);
    ++frame;
    if (frame > 3)
      frame = 0;
    ++tries;
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.setCursor(0, 20);
    tft.print("WIFI: ");
    tft.println(WiFi.localIP());
  } else {
    tft.setCursor(0, 20);
    tft.print("WIFI: ");
    tft.println("Failed!");
  }

  tft.setCursor(0, 40);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("MDNS: ");
  if (MDNS.begin(hostname)) {
    tft.setCursor(0, 40);
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.print("MDNS: ");
    tft.println(hostname);
  } else {
    tft.println("Failed");
  }

  tft.setCursor(0, 60);
  tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
  tft.print("NTP: ");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  update_tft_time();

  tft.setCursor(0, 80);
  tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
  tft.print("MCP: ");

  if (monitor.mcp_begin()) {
    tft.setCursor(0, 80);
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.println("MCP: Found");
  } else {
    tft.setTextColor(ST77XX_RED, BG_COLOR);
    tft.println("Not found");
  }

  tft.setCursor(0, 100);
  tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
  tft.print("LFS: ");
  if(LittleFS.begin(false)) {
    tft.setCursor(0, 100);
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.print("LFS: Mounted");
  } else {
    tft.setTextColor(ST77XX_RED, BG_COLOR);
    tft.println("Mount failed");
  }

  web_server.begin();

  // Don't timeout screen during / after setup
  monitor.reset_direct_input_timeout();
  monitor.check_door_sensor();

  led_ring.init();
}

/* --------------------------------------------------------------------------- */


void set_backlight(bool state) {
  backlight = state;
  digitalWrite(TFT_BACKLIGHT, backlight);
}

void loop() {
    digitalWrite(LED_BUILTIN, millis() % 1024 < 512);

    web_server.handleClient();

    monitor.check_for_motion();
    monitor.check_door_sensor();
    if (monitor.check_for_button_input()) {
	if (!backlight)
	    set_backlight(true);
    } else if (backlight && monitor.direct_input_timeout_past())
	set_backlight(false);

    monitor.update_outputs();

    if (backlight) {
	EVERY_N_MILLISECONDS(500) {
	    sensors_event_t humidity, temp;
	    monitor.getAHTEvent(humidity, temp);

	    tft.setCursor(0, 0);
	    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
	    tft.print("AHT20: ");
	    tft.print(temp.temperature * 9 / 5 + 32, 0);
	    tft.print(" F ");
	    tft.print(humidity.relative_humidity, 0);
	    tft.print(" %");
	    tft.println("              ");

	    tft.setCursor(0, 120);
	    tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
	    tft.print("Ambient light: ");
	    uint16_t analogread = analogRead(A3);
	    tft.setTextColor(ST77XX_WHITE, BG_COLOR);
	    tft.print(analogread);
	    tft.println("    ");

	    tft.setCursor(0, 140);
	    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
	    tft.print("LED level:");
	    char ledstr[10];
	    snprintf(ledstr, 10, "% 3d/%d ", strip_controller.brightness(), strip_controller.max_brightness());
	    tft.println(ledstr);

	    update_tft_time();
	}
    }

    delay(1);
}

/* --------------------------------------------------------------------------- */
