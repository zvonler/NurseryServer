
#include "funhouse_screen.h"
#include "led_ring.h"
#include "led_strip_controller.h"
#include "nursery_monitor.h"
#include "nursery_web_server.h"
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <time.h>

/*------ File included next should define SECRET_SSID and SECRET_PASS -------*/
#include "arduino_secrets.h"
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
/*---------------------------------------------------------------------------*/

LEDStripController strip_controller(A0, A1);
LEDRing led_ring;
NurseryMonitor monitor(strip_controller, led_ring);
NurseryWebServer web_server(strip_controller, led_ring, LittleFS, monitor);
FunHouseScreen screen;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -6 * 3600;
const int daylightOffset_sec = 3600;

// Change for "release" builds
const char* hostname = "nursery-devel";

/*---------------------------------------------------------------------------*/

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);

    monitor.init();
    strip_controller.init();
    screen.init();
    led_ring.init();

    monitor.check_door_sensor();

    if (!monitor.aht_begin()) {
        screen.print_row(FunHouseScreen::AHT, ST77XX_RED, "AHT20: FAIL!");
    } else {
        screen.print_row(FunHouseScreen::AHT, ST77XX_GREEN, "AHT20: OK!");
    }

    screen.print_row(FunHouseScreen::WIFI, ST77XX_YELLOW, "WIFI: ");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 30) {
        const char* frames[4] = { "|", "/", "-", "\\" };
        static int frame = 0;
        String text = String("WIFI: ") + String(frames[frame++]);
        screen.print_row(FunHouseScreen::WIFI, ST77XX_YELLOW, text);
        if (frame > 3)
            frame = 0;
        ++tries;
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
        String text = String("WIFI: ") + WiFi.localIP().toString();
        screen.print_row(FunHouseScreen::WIFI, ST77XX_GREEN, text);
    } else {
        screen.print_row(FunHouseScreen::WIFI, ST77XX_RED, "WIFI: Failed!");
    }

    screen.print_row(FunHouseScreen::MDNS, ST77XX_YELLOW, "MDNS: ");
    if (MDNS.begin(hostname)) {
        String text = String("MDNS: ") + String(hostname);
        screen.print_row(FunHouseScreen::MDNS, ST77XX_GREEN, text);
    } else {
        screen.print_row(FunHouseScreen::MDNS, ST77XX_RED, "MDNS: Failed");
    }

    screen.print_row(FunHouseScreen::NTP, ST77XX_YELLOW, "NTP: ");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    screen.print_row(FunHouseScreen::MCP, ST77XX_YELLOW, "MCP: ");
    if (monitor.mcp_begin()) {
        screen.print_row(FunHouseScreen::MCP, ST77XX_GREEN, "MCP: Found");
    } else {
        screen.print_row(FunHouseScreen::MCP, ST77XX_RED, "MCP: Not found");
    }

    screen.print_row(FunHouseScreen::LFS, ST77XX_YELLOW, "LFS: ");
    if(LittleFS.begin(false)) {
        screen.print_row(FunHouseScreen::LFS, ST77XX_GREEN, "LFS: Mounted");
    } else {
        screen.print_row(FunHouseScreen::LFS, ST77XX_RED, "LFS: Failed");
    }

    web_server.begin();

    // Don't timeout screen during / after setup
    monitor.reset_direct_input_timeout();
}

/*---------------------------------------------------------------------------*/

void loop()
{
    uint32_t now = millis();

    digitalWrite(LED_BUILTIN, now % 1024 < 512);

    web_server.handleClient();

    monitor.check_for_motion();
    monitor.check_door_sensor();
    if (monitor.check_for_button_input()) {
        if (!screen.backlight_on())
            screen.set_backlight(true);
    } else if (screen.backlight_on() && monitor.direct_input_timeout_past()) {
        screen.set_backlight(false);
    }

    monitor.update_outputs(now);

    if (screen.backlight_on()) {
        EVERY_N_MILLISECONDS(500) {
            char buf[48];

            struct tm timeinfo;
            if (!getLocalTime(&timeinfo)) {
                screen.print_row(FunHouseScreen::NTP, ST77XX_RED, "NTP: Failed");
            } else {
                strftime(buf, 48, "NTP: %Y%m%d %H:%M", &timeinfo);
                screen.print_row(FunHouseScreen::NTP, ST77XX_GREEN, buf);
            }

            sensors_event_t humidity, temp;
            monitor.getAHTEvent(humidity, temp);
            snprintf(buf, 48, "AHT20: %d F %d %%",
                int(temp.temperature * 9 / 5 + 32),
                int(humidity.relative_humidity));
            screen.print_row(FunHouseScreen::AHT, ST77XX_GREEN, buf);

            uint16_t analogread = analogRead(A3);
            snprintf(buf, 48, "Ambient light: %d", analogread);
            screen.print_row(FunHouseScreen::AMBIENT, ST77XX_GREEN, buf);

            snprintf(buf, 48, "LED level: %d/%d ", strip_controller.brightness(), strip_controller.max_brightness());
            screen.print_row(FunHouseScreen::LED_STRIP_LEVEL, ST77XX_GREEN, buf);

            if (led_ring.in_timeout(now)) {
                snprintf(buf, 48, "Timeout: %d secs", led_ring.timeout_millis_remaining(now)/1000);
                screen.print_row(FunHouseScreen::TIMEOUT, ST77XX_RED, buf);
            } else {
                screen.print_row(FunHouseScreen::TIMEOUT, ST77XX_GREEN, "Timeout: Inactive");
            }
        }
    }

    delay(1);
}

/*---------------------------------------------------------------------------*/
