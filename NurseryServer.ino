/**
   Smart Nursery Server

   Target: Adafruit FunHouse

   The buttons on the FunHouse map to:
     Top - increase main lights brightness
     Middle - toggle display backlight
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

   The FunHouse I2C connection is used to talk to MCP23008 to read the RF remote
   receiver signals.
*/

#include <Adafruit_AHTX0.h>
#include <Adafruit_GFX.h>
#include <Adafruit_MCP23008.h>
#include <Adafruit_ST7789.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <time.h>

/* --------------------------------------------------------------------------- */

#define BG_COLOR ST77XX_BLACK
#define LED_CHANNEL_0 A0
#define LED_CHANNEL_1 A1

#define LED_REFRESH_HZ 40000
#define LED_RESOLUTION_BITS 8
#define MAX_BRIGHTNESS 250
#define INITIAL_BRIGHTNESS 20
#define BRIGHTNESS_STEP 50

Adafruit_MCP23008 mcp;
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RESET);
Adafruit_AHTX0 aht;

// MCP pin assignments
#define REMOTE_BRIGHTER 5
#define REMOTE_DIMMER 4
#define REMOTE_OFF 6
#define REMOTE_WAKE 7
#define DOOR_SENSOR 3

bool waking_up = false;
uint32_t wakeup_start_tm = 0;
int brightness = 0;
bool backlight = true;
bool pir_triggered = false;
bool mcp_found = false;
uint32_t last_direct_input_tm = 0;
bool door_closed;
struct tm last_light_change_timeinfo;
struct tm last_door_change_timeinfo;
struct tm last_motion_timeinfo;

// -------- File included below should define SECRET_SSID and SECRET_PASS --------
#include "arduino_secrets.h"
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;
// -------------------------------------------------------------------------------

WebServer server(80);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -6 * 3600;
const int   daylightOffset_sec = 3600;

// Change for "release" builds
const char* hostname = "nursery-dev";

/* --------------------------------------------------------------------------- */

void handle_root() {
  constexpr size_t bufsize = 1024;
  char buf[bufsize];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  char timestr[128];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
    strftime(timestr, 128, "%A %d %B %Y %H:%M:%S", &timeinfo);

  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  char motionstr[128];
  strftime(motionstr, 128, "%H:%M:%S", &last_motion_timeinfo);

  char doorstr[128];
  strftime(doorstr, 128, "%H:%M:%S", &last_door_change_timeinfo);

  char lightstr[128];
  strftime(lightstr, 128, "%H:%M:%S", &last_light_change_timeinfo);

  snprintf(buf, bufsize,
           "<html>\
  <head>\
    <meta http-equiv='refresh' content='15'/>\
    <title>Smart Nursery Home Page</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Smart Nursery</h1>\
    <p><a href=/off>OFF</a>    <a href=/dimmer>DIMMER</a>    <a href=/brighter>BRIGHTER</a>    <a href=/wake>WAKE</a></p>\
    <p>%s</p>\
    <p>Lights are: %s at %d / %d since %s</p>\
    <p>Door is %s since %s</p>\
    <p>Last motion detected: %s</p>\
    <p>Temperature: %.0f F    Humidity: %.0f %%</p>\
    <p>Server Uptime: % 3d:%02d:%02d</p>\
  </body>\
</html>",

           timestr,
           brightness > 0 ? "ON" : "OFF", brightness, MAX_BRIGHTNESS, lightstr,
           door_closed ? "CLOSED" : "OPEN", doorstr,
           motionstr,
           temp.temperature * 9 / 5 + 32, humidity.relative_humidity,
           hr, min % 60, sec % 60
          );
  server.send(200, "text/html", buf);
}

void send_client_redirect(const char* title)
{
  constexpr size_t bufsize = 1024;
  char buf[bufsize];

  snprintf(buf, bufsize, "\
    <html>\
      <head>\
        <meta http-equiv='refresh' content=\"1; URL='/'\">\
        <title>%s</title>\
      </head>\
    </html>\
  ", title);

  server.send(200, "text/html", buf);
}

void handle_brighter()
{
  increase_brightness();
  send_client_redirect("Increasing brightness");
}

void handle_dimmer()
{
  decrease_brightness();
  send_client_redirect("Decreasing brightness");
}

void handle_off()
{
  turn_off();
  send_client_redirect("Turning OFF");
}

void handle_wake()
{
  begin_wake();
  send_client_redirect("Waking up");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

/* --------------------------------------------------------------------------- */

void update_tft_time()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    tft.setCursor(0, 60);
    tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
    tft.print("NTP: ");
    tft.setTextColor(ST77XX_RED, BG_COLOR);
    tft.println("Failed");
  }
  else
  {
    tft.setCursor(0, 60);
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.print("NTP: ");
    char timestr[128];
    strftime(timestr, 128, "%Y%m%d %H:%M", &timeinfo);
    tft.println(timestr);
  }
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(BUTTON_SELECT, INPUT_PULLDOWN);
  pinMode(BUTTON_UP, INPUT_PULLDOWN);
  pinMode(SENSOR_PIR, INPUT);
  pinMode(SENSOR_LIGHT, INPUT);
  analogReadResolution(10);

  ledcAttachPin(LED_CHANNEL_0, 0);
  ledcSetup(0, LED_REFRESH_HZ, LED_RESOLUTION_BITS);

  ledcAttachPin(LED_CHANNEL_1, 1);
  ledcSetup(1, LED_REFRESH_HZ, LED_RESOLUTION_BITS);

  tft.init(240, 240);                // Initialize ST7789 screen
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH); // Backlight on

  tft.fillScreen(BG_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextWrap(false);

  // check AHT!
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("AHT20: ");

  if (! aht.begin()) {
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

  if (WiFi.status() == WL_CONNECTED)
  {
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.setCursor(0, 20);
    tft.print("WIFI: ");
    tft.println(WiFi.localIP());
  }
  else
  {
    tft.setCursor(0, 20);
    tft.print("WIFI: ");
    tft.println("Failed!");
  }

  tft.setCursor(0, 40);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("MDNS: ");
  if (MDNS.begin(hostname))
  {
    tft.setCursor(0, 40);
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.print("MDNS: ");
    tft.println(hostname);
  }
  else
  {
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

  if (mcp.begin())
  {
    mcp_found = true;

    mcp.pinMode(REMOTE_BRIGHTER, INPUT);
    mcp.pullUp(REMOTE_BRIGHTER, LOW);

    mcp.pinMode(REMOTE_DIMMER, INPUT);
    mcp.pullUp(REMOTE_DIMMER, LOW);

    mcp.pinMode(REMOTE_OFF, INPUT);
    mcp.pullUp(REMOTE_OFF, LOW);

    mcp.pinMode(REMOTE_WAKE, INPUT);
    mcp.pullUp(REMOTE_WAKE, LOW);

    mcp.pinMode(DOOR_SENSOR, INPUT);
    mcp.pullUp(DOOR_SENSOR, HIGH);

    tft.setCursor(0, 80);
    tft.setTextColor(ST77XX_GREEN, BG_COLOR);
    tft.println("MCP: Found");
  }
  else
  {
    tft.setTextColor(ST77XX_RED, BG_COLOR);
    tft.println("Not found");
  }

  server.on("/", handle_root);
  server.on("/brighter", handle_brighter);
  server.on("/dimmer", handle_dimmer);
  server.on("/off", handle_off);
  server.on("/wake", handle_wake);
  server.onNotFound(handleNotFound);
  server.begin();

  // Don't timeout screen during / after setup
  last_direct_input_tm = millis();

  door_closed = !mcp.digitalRead(DOOR_SENSOR);
}

/* --------------------------------------------------------------------------- */

uint32_t last_light_change_ms = 0;

void increase_brightness()
{
  if (!brightness)
    brightness = INITIAL_BRIGHTNESS;
  else
    brightness += BRIGHTNESS_STEP;

  if (brightness > MAX_BRIGHTNESS)
    brightness = MAX_BRIGHTNESS;
  waking_up = false;
  getLocalTime(&last_light_change_timeinfo);
  last_light_change_ms = millis();
}

void decrease_brightness()
{
  brightness -= BRIGHTNESS_STEP;
  if (brightness < 0)
    brightness = 0;
  waking_up = false;
  getLocalTime(&last_light_change_timeinfo);
  last_light_change_ms = millis();
}

void begin_wake()
{
  waking_up = true;
  wakeup_start_tm = millis();
  last_light_change_ms = millis();
}

void turn_off()
{
  brightness = 0;
  waking_up = false;
  getLocalTime(&last_light_change_timeinfo);
}

void set_backlight(bool state)
{
  backlight = state;
  digitalWrite(TFT_BACKLIGHT, backlight);
}

void check_for_motion()
{
  if (digitalRead(SENSOR_PIR))
  {
    pir_triggered = true;
  }
  else
  {
    if (pir_triggered)
    {
      pir_triggered = false;
      getLocalTime(&last_motion_timeinfo);
    }
  }
}

void loop()
{
  digitalWrite(LED_BUILTIN, millis() % 1024 < 512);

  server.handleClient();

  check_for_motion();

  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_GREEN, BG_COLOR);
  tft.print("AHT20: ");
  tft.print(temp.temperature * 9 / 5 + 32, 0);
  tft.print(" F ");
  tft.print(humidity.relative_humidity, 0);
  tft.print(" %");
  tft.println("              ");

  tft.setCursor(0, 100);
  tft.setTextColor(ST77XX_YELLOW, BG_COLOR);
  tft.print("Ambient light: ");
  uint16_t analogread = analogRead(A3);
  tft.setTextColor(ST77XX_WHITE, BG_COLOR);
  tft.print(analogread);
  tft.println("    ");

  tft.setCursor(0, 120);
  tft.setTextColor(ST77XX_GREEN, BG_COLOR);
  tft.print("LED level:");
  char ledstr[10];
  snprintf(ledstr, 10, "% 3d/%d ", brightness, MAX_BRIGHTNESS);
  tft.println(ledstr);

  update_tft_time();

  if (digitalRead(BUTTON_DOWN))
  {
    if (!backlight)
      set_backlight(true);
    decrease_brightness();
    while (digitalRead(BUTTON_DOWN));
    last_direct_input_tm = millis();
  }

  if (digitalRead(BUTTON_SELECT))
  {
    set_backlight(!backlight);
    while (digitalRead(BUTTON_SELECT));
    last_direct_input_tm = millis();
  }

  if (digitalRead(BUTTON_UP))
  {
    if (!backlight)
      set_backlight(true);
    increase_brightness();
    while (digitalRead(BUTTON_UP));
    last_direct_input_tm = millis();
  }

  if (mcp_found)
  {
    static uint32_t last_remote_tm = 0;
    if (millis() - last_remote_tm > 500)
    {
      if (mcp.digitalRead(REMOTE_BRIGHTER))
        increase_brightness(), last_remote_tm = millis();
      else if (mcp.digitalRead(REMOTE_DIMMER))
        decrease_brightness(), last_remote_tm = millis();
      else if (mcp.digitalRead(REMOTE_WAKE))
        begin_wake(), last_remote_tm = millis();
      else if (mcp.digitalRead(REMOTE_OFF))
        turn_off(), last_remote_tm = millis();
    }

    if (mcp.digitalRead(DOOR_SENSOR))
    {
      if (door_closed)
      {
        getLocalTime(&last_door_change_timeinfo);
        door_closed = false;
      }
    }
    else
    {
      if (!door_closed)
      {
        getLocalTime(&last_door_change_timeinfo);
        door_closed = true;
      }
    }
  }

  if (waking_up)
  {
    brightness = (millis() - wakeup_start_tm) / 2000;
    if (brightness >= 2 * BRIGHTNESS_STEP)
      waking_up = false;
    getLocalTime(&last_light_change_timeinfo);
  }
  else if (brightness && millis() - last_light_change_ms > 3600000 * 2) // 2 hours
  {
    brightness = 0;
    getLocalTime(&last_light_change_timeinfo);
    last_light_change_ms = millis();
  }

  if (backlight && millis() - last_direct_input_tm > 10000)
    set_backlight(false);

  ledcWrite(0, brightness);
  ledcWrite(1, brightness > BRIGHTNESS_STEP ? brightness : 0);

  delay(10);
}

/* --------------------------------------------------------------------------- */
