
#ifndef nursery_monitor_h
#define nursery_monitor_h

#include "led_ring.h"
#include "led_strip_controller.h"
#include <Adafruit_AHTX0.h>
#include <Adafruit_MCP23008.h>
#include <ArduinoJson.h>
#include <time.h>

/*---------------------------------------------------------------------------*/

/**
 * Monitors the FunHouse sensor inputs.
 */
class NurseryMonitor {
    LEDStripController& _strip_controller;
    LEDRing& _ring_controller;
    Adafruit_MCP23008 _mcp;
    Adafruit_AHTX0 _aht;
    bool _pir_triggered = false;
    bool _mcp_found = false;
    uint32_t _last_direct_input_tm = 0;
    bool _door_closed = false;
    struct tm _last_door_change_timeinfo;
    struct tm _last_motion_timeinfo;

    // MCP pin assignments
    static const uint8_t DOOR_SENSOR = 3;
    static const uint8_t REMOTE_A = 4;
    static const uint8_t REMOTE_B = 5;
    static const uint8_t REMOTE_D = 6;
    static const uint8_t REMOTE_C = 7;

public:
    NurseryMonitor(LEDStripController& strip_controller, LEDRing& ring_controller)
        : _strip_controller(strip_controller)
        , _ring_controller(ring_controller)
    { }

    void init()
    {
        pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
        pinMode(BUTTON_SELECT, INPUT_PULLDOWN);
        pinMode(BUTTON_UP, INPUT_PULLDOWN);
        pinMode(SENSOR_PIR, INPUT);
        pinMode(SENSOR_LIGHT, INPUT);
        analogReadResolution(10);
    }

    bool aht_begin() { return _aht.begin(); }

    bool mcp_begin()
    {
        if (_mcp.begin()) {
            _mcp_found = true;

            _mcp.pinMode(REMOTE_B, INPUT);
            _mcp.pullUp(REMOTE_B, LOW);

            _mcp.pinMode(REMOTE_A, INPUT);
            _mcp.pullUp(REMOTE_A, LOW);

            _mcp.pinMode(REMOTE_D, INPUT);
            _mcp.pullUp(REMOTE_D, LOW);

            _mcp.pinMode(REMOTE_C, INPUT);
            _mcp.pullUp(REMOTE_C, LOW);

            _mcp.pinMode(DOOR_SENSOR, INPUT);
            _mcp.pullUp(DOOR_SENSOR, HIGH);
        }
        return _mcp_found;
    }

    void add_status(StaticJsonDocument<1024>& doc)
    {
        char timestr[128];
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
            strftime(timestr, 128, "%A %d %B %Y %H:%M:%S", &timeinfo);
        doc["time"] = timestr;

        sensors_event_t humidity, temp;
        getAHTEvent(humidity, temp);
        doc["humidity"] = int(humidity.relative_humidity);
        doc["temperature"] = int(temp.temperature * 9 / 5 + 32);

        char motionstr[128];
        strftime(motionstr, 128, "%H:%M:%S", &_last_motion_timeinfo);
        doc["last_motion_time"] = motionstr;

        char doorstr[128];
        strftime(doorstr, 128, "%H:%M:%S", &_last_door_change_timeinfo);
        doc["last_door_time"] = doorstr;
        doc["door_status"] = _door_closed ? "CLOSED" : "OPEN";

        char uptime[24];
        int sec = millis() / 1000;
        int min = sec / 60;
        int hr = min / 60;
        sprintf(uptime, "% 3d:%02d:%02d", hr, min % 60, sec % 60);
        doc["server_uptime"] = uptime;

        uint32_t now = millis();
        if (_ring_controller.in_timeout(now)) {
            doc["timeout"] = String(_ring_controller.timeout_millis_remaining(now)/1000) + " seconds remaining";
        } else {
            doc["timeout"] = "inactive";
        }
    }

    void reset_direct_input_timeout() { _last_direct_input_tm = millis(); }

    bool direct_input_timeout_past() { return millis() - _last_direct_input_tm > 10000; }

    void check_for_motion()
    {
        if (digitalRead(SENSOR_PIR)) {
            _pir_triggered = true;
        } else {
            if (_pir_triggered) {
                _pir_triggered = false;
                getLocalTime(&_last_motion_timeinfo);
            }
        }
    }

    void check_door_sensor()
    {
        if (!_mcp_found) return;

        if (_mcp.digitalRead(DOOR_SENSOR)) {
            if (_door_closed)
                toggle_door_closed();
        } else {
            if (!_door_closed)
                toggle_door_closed();
        }
    }

    bool check_for_button_input() {
        if (digitalRead(BUTTON_DOWN)) {
            _strip_controller.decrease_brightness();
            while (digitalRead(BUTTON_DOWN))
                ;
        } else if (digitalRead(BUTTON_SELECT)) {
            if (_ring_controller.mode() != LEDRing::TIMEOUT)
                _ring_controller.setMode(LEDRing::TIMEOUT);
            else
                _ring_controller.setMode(LEDRing::OFF);
            while (digitalRead(BUTTON_SELECT))
                ;
        } else if (digitalRead(BUTTON_UP)) {
            _strip_controller.increase_brightness();
            while (digitalRead(BUTTON_UP))
                ;
        } else {
            return false;
        }

        _last_direct_input_tm = millis();
        return true;
    }

    void getAHTEvent(sensors_event_t& humidity, sensors_event_t& temp)
    {
        _aht.getEvent(&humidity, &temp);
    }

    void update_outputs(uint32_t tm)
    {
        update_ring(tm);
        _strip_controller.update();
    }

private:
    void update_ring(uint32_t tm)
    {
        if (_ring_controller.in_timeout(tm)) {
            // While timeout is active, let the ring manage its state
        } else if (_strip_controller.lights_off()) {
            // If main lights are off then ring should be off unless a timeout just finished.
            if (_ring_controller.mode() != LEDRing::TIMEOUT || _ring_controller.timeout_millis_past(tm) > 60000)
                _ring_controller.setMode(LEDRing::OFF);
        } else if (_mcp_found) {
            // Not in timeout, main lights on
            static uint32_t last_remote_tm = 0;
            if (tm - last_remote_tm > 500) {
                if (_mcp.digitalRead(REMOTE_A))
                    _ring_controller.setMode(LEDRing::CONFETTI), last_remote_tm = tm;
                else if (_mcp.digitalRead(REMOTE_B))
                    _ring_controller.setMode(LEDRing::PULSE), last_remote_tm = tm;
                else if (_mcp.digitalRead(REMOTE_C))
                    _ring_controller.setMode(LEDRing::CANDLE), last_remote_tm = tm;
                else if (_mcp.digitalRead(REMOTE_D))
                    _ring_controller.setMode(LEDRing::OFF), last_remote_tm = tm;
            }
        }
        _ring_controller.update();
    }

    void toggle_door_closed()
    {
        _door_closed = !_door_closed;
        getLocalTime(&_last_door_change_timeinfo);
    }
};

/*---------------------------------------------------------------------------*/

#endif
