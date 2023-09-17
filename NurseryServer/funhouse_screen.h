
#ifndef funhouse_screen_h
#define funhouse_screen_h

#include <Adafruit_ST7789.h>

/*---------------------------------------------------------------------------*/

/**
 * Divides the FunHouse TFT Screen into enumerated rows.
 */
class FunHouseScreen {
    Adafruit_ST7789 _tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RESET);
    bool _backlight_on = true;

    static const int TEXT_SIZE = 2;
    static const int ROW_HEIGHT = 20; // For TextSize 2
    static const uint16_t BG_COLOR = ST77XX_BLACK;
    const String SPACES = "                    "; // 20 spaces

public:
    enum Row {
        AHT,
        WIFI,
        MDNS,
        NTP,
        MCP,
        LFS,
        AMBIENT,
        LED_STRIP_LEVEL,
        TIMEOUT,
    };

    void init() {
        _tft.init(240, 240);  // Initialize ST7789 screen
        pinMode(TFT_BACKLIGHT, OUTPUT);
        digitalWrite(TFT_BACKLIGHT, HIGH);  // Backlight on

        _tft.fillScreen(BG_COLOR);
        _tft.setTextSize(TEXT_SIZE);
        _tft.setTextColor(ST77XX_YELLOW);
        _tft.setTextWrap(false);
    }

    bool backlight_on() const { return _backlight_on; }

    void set_backlight(bool state) {
        _backlight_on = state;
        digitalWrite(TFT_BACKLIGHT, _backlight_on);
    }

    void print_row(Row row, uint16_t color, String text) {
        _tft.setCursor(0, row * ROW_HEIGHT);
        _tft.setTextColor(color, BG_COLOR);
        if (text.length() < 20)
            text += SPACES.substring(0, 20 - text.length());
        _tft.println(text);
    }
};

/*---------------------------------------------------------------------------*/

#endif

