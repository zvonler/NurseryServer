
#ifndef led_strip_controller_h
#define led_strip_controller_h

#include <ArduinoJson.h>
#include <time.h>

/*---------------------------------------------------------------------------*/

/**
 * Manages two PWM channels that control 12v LED strips.
 */
class LEDStripController {
    static const int LED_REFRESH_HZ = 40000;
    static const int LED_RESOLUTION_BITS = 8;
    static const int MAX_BRIGHTNESS = 250;
    static const int INITIAL_BRIGHTNESS = 20;
    static const int BRIGHTNESS_STEP = 50;
    static const int IDLE_TIMEOUT = 3600000 * 2; // 2 hours

    uint8_t _pins[2];
    bool _waking_up = false;
    uint32_t _wakeup_start_tm = 0;
    int _brightness = 0;
    struct tm _last_light_change_timeinfo;
    uint32_t _last_light_change_ms = 0;

public:
    LEDStripController(uint8_t pin0, uint8_t pin1)
	: _pins({ pin0, pin1 })
    { }

    bool lights_off() const { return _brightness == 0; }
    int brightness() const { return _brightness; }
    int max_brightness() const { return MAX_BRIGHTNESS; }

    void init()
    {
      ledcAttachPin(_pins[0], 0);
      ledcSetup(0, LED_REFRESH_HZ, LED_RESOLUTION_BITS);

      ledcAttachPin(_pins[1], 1);
      ledcSetup(1, LED_REFRESH_HZ, LED_RESOLUTION_BITS);
    }

    void update()
    {
	if (_waking_up) {
	    _brightness = (millis() - _wakeup_start_tm) / 2000;
	    if (_brightness >= 2 * BRIGHTNESS_STEP)
		_waking_up = false;
	    getLocalTime(&_last_light_change_timeinfo);
	} else if (_brightness && millis() - _last_light_change_ms > IDLE_TIMEOUT) {
	    _brightness = 0;
	    getLocalTime(&_last_light_change_timeinfo);
	    _last_light_change_ms = millis();
	}

	ledcWrite(0, _brightness);
	ledcWrite(1, _brightness > BRIGHTNESS_STEP ? _brightness : 0);
    }

    void increase_brightness()
    {
	if (!_brightness)
	    _brightness = INITIAL_BRIGHTNESS;
	else
	    _brightness += BRIGHTNESS_STEP;

	if (_brightness > MAX_BRIGHTNESS)
	    _brightness = MAX_BRIGHTNESS;
	_waking_up = false;
	getLocalTime(&_last_light_change_timeinfo);
	_last_light_change_ms = millis();
    }

    void decrease_brightness()
    {
	_brightness -= BRIGHTNESS_STEP;
	if (_brightness < 0)
	    _brightness = 0;
	_waking_up = false;
	getLocalTime(&_last_light_change_timeinfo);
	_last_light_change_ms = millis();
    }

    void begin_wake()
    {
	_waking_up = true;
	_wakeup_start_tm = millis();
	_last_light_change_ms = millis();
    }

    void turn_off()
    {
	_brightness = 0;
	_waking_up = false;
	getLocalTime(&_last_light_change_timeinfo);
    }

    void add_status(StaticJsonDocument<1024>& doc)
    {
      doc["brightness"] = _brightness;
      doc["waking_up"] = _waking_up;
      char lightstr[128];
      strftime(lightstr, 128, "%H:%M:%S", &_last_light_change_timeinfo);
      doc["last_light_time"] = lightstr;
  }
};

/*---------------------------------------------------------------------------*/

#endif
