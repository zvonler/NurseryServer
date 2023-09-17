
#ifndef led_ring_h
#define led_ring_h

#include <FastLED.h>

FASTLED_USING_NAMESPACE

// Define this value before including this file to override
#ifndef LED_RING_PIN
    #define LED_RING_PIN A2
#endif

/*---------------------------------------------------------------------------*/

/**
 * Controls an LED ring with 36 pixels.
 * Original project: https://www.instructables.com/A-Minimalist-LED-Lamp/
 */
class LEDRing
{
public:
    enum Mode
    {
        OFF,
        PULSE,
        CONFETTI,
        CANDLE,
        TIMEOUT,
    };

private:
    static const int NUM_LEDS = 36;
    static const int BRIGHTNESS = 40;
    static const int FRAMES_PER_SECOND = 120;
    static const uint32_t TIMEOUT_DURATION = 180000;

    // A dummy pixel is used as a level shifter
    CRGB _leds_with_dummy[NUM_LEDS + 1];
    CRGB *_leds;
    Mode _mode = LEDRing::OFF;
    uint32_t _timeout_start_ms = 0;

public:
    LEDRing()
        : _leds(_leds_with_dummy + 1)
    {
        _leds_with_dummy[0] = 0;
    }

    void init()
    {
        FastLED.addLeds<WS2811, LED_RING_PIN, GRB>(_leds_with_dummy, NUM_LEDS + 1).setCorrection(TypicalLEDStrip);
        FastLED.setBrightness(BRIGHTNESS);
    }

    Mode mode() const { return _mode; }

    void setMode(Mode mode)
    {
        if (mode == LEDRing::TIMEOUT)
            _timeout_start_ms = millis();
        _mode = mode;
    }

    void update()
    {
        if (_mode == LEDRing::PULSE)
            pulse();
        else if (_mode == LEDRing::CANDLE)
            Fire2012WithPalette();
        else if (_mode == LEDRing::CONFETTI)
            confetti();
        else if (_mode == LEDRing::TIMEOUT)
            timeout();
        else
            fill_solid(_leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
    }

    bool in_timeout(uint32_t tm) const
    {
        return _mode == LEDRing::TIMEOUT && tm < _timeout_start_ms + TIMEOUT_DURATION;
    }

    uint32_t timeout_millis_remaining(uint32_t tm) const
    {
        if (in_timeout(tm))
            return TIMEOUT_DURATION - (tm - _timeout_start_ms);
        return 0;
    }

private:
    void timeout()
    {
        if (!in_timeout(millis())) {
            fill_solid(_leds, NUM_LEDS, CRGB::Green);
        } else {
            float frac_remaining = 1.0 - (millis() - _timeout_start_ms) / (float)TIMEOUT_DURATION;
            int num_red = frac_remaining * NUM_LEDS + 0.5;
            if (num_red > NUM_LEDS)
                num_red = NUM_LEDS;
            else if (num_red < 1)
                num_red = 1; // Keep at least one always on so it's not dark during the transition to green
            fill_solid(_leds, num_red, CRGB::Red);
            fill_solid(_leds + num_red, NUM_LEDS - num_red, CRGB::Black);
        }
    }

    void pulse()
    {
        static uint8_t red_bpm = 9;
        static uint8_t green_bpm = 7;
        static uint8_t blue_bpm = 3;

        uint8_t red_limit = beatsin8(9, 16, 128, 0);
        uint8_t green_limit = beatsin8(11, 16, 128, 5000);
        uint8_t blue_limit = beatsin8(13, 16, 128, 10000);

        for (int i = 0; i < NUM_LEDS; i++) {
            uint8_t phase_offset = i * (255 / NUM_LEDS);
            uint8_t red = beatsin8(red_bpm, 0, 255, 0, phase_offset);
            red = red < red_limit ? 0 : red - red_limit;
            uint8_t green = beatsin8(green_bpm, 0, 255, 5000, 255 - phase_offset);
            green = green < green_limit ? 0 : green - green_limit;
            uint8_t blue = beatsin8(blue_bpm, 0, 255, 0, phase_offset);
            blue = blue < blue_limit ? 0 : blue - blue_limit;

            _leds[i] = CRGB(red, green, blue);
        }
    }

    void confetti()
    {
        // random colored speckles that blink in and fade smoothly
        static uint8_t gHue = 0;
        EVERY_N_MILLISECONDS(20) {
            gHue++;
        }  // slowly cycle the "base color" through the rainbow
        fadeToBlackBy(_leds, NUM_LEDS, 10);
        int pos = random16(NUM_LEDS);
        _leds[pos] += CHSV(gHue + random8(64), 200, 255);
    }

    // Fire2012 by Mark Kriegsman, July 2012
    // as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
    ////
    // This basic one-dimensional 'fire' simulation works roughly as follows:
    // There's a underlying array of 'heat' cells, that model the temperature
    // at each point along the line.  Every cycle through the simulation,
    // four steps are performed:
    //  1) All cells cool down a little bit, losing heat to the air
    //  2) The heat from each cell drifts 'up' and diffuses a little
    //  3) Sometimes randomly new 'sparks' of heat are added at the bottom
    //  4) The heat from each cell is rendered as a color into the leds array
    //     The heat-to-color mapping uses a black-body radiation approximation.
    //
    // Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
    //
    // This simulation scales it self a bit depending on NUM_LEDS; it should look
    // "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
    //
    // I recommend running this simulation at anywhere from 30-100 frames per second,
    // meaning an interframe delay of about 10-35 milliseconds.
    //
    // Looks best on a high-density LED setup (60+ pixels/meter).
    //
    //
    // There are two main parameters you can play with to control the look and
    // feel of your fire: COOLING (used in step 1 above), and SPARKING (used
    // in step 3 above).
    //
    // COOLING: How much does the air cool as it rises?
    // Less cooling = taller flames.  More cooling = shorter flames.
    // Default 50, suggested range 20-100
    static const int COOLING = 50;

    // SPARKING: What chance (out of 255) is there that a new spark will be lit?
    // Higher chance = more roaring fire.  Lower chance = more flickery fire.
    // Default 120, suggested range 50-200.
    static const int SPARKING = 60;

    void Fire2012WithPalette()
    {
        // Array of temperature readings at each simulation cell
        static byte heat_0[NUM_LEDS / 2];
        static byte heat_1[NUM_LEDS / 2];

        EVERY_N_MILLISECONDS(25) {
            // Step 1.  Cool down every cell a little
            for (int i = 0; i < NUM_LEDS/2; i++) {
                heat_0[i] = qsub8(heat_0[i], random8(0, ((COOLING * 10) / NUM_LEDS / 2) + 2));
                heat_1[i] = qsub8(heat_1[i], random8(0, ((COOLING * 10) / NUM_LEDS / 2) + 2));
            }

            // Step 3.1. Animate the coals at the bottom
            constexpr int min_coal_temp = 25;
            for (int i = 0; i < 4; ++i) {
                heat_0[i] = max(min_coal_temp, heat_0[i] + random8(0, ((COOLING * 10) / NUM_LEDS / 2)));
                heat_1[i] = max(min_coal_temp, heat_1[i] + random8(0, ((COOLING * 10) / NUM_LEDS / 2)));
            }
        }

        EVERY_N_MILLISECONDS(25) {

            // Step 2.  Heat from each cell drifts 'up' and diffuses a little
            for (int k = NUM_LEDS/2 - 1; k >= 2; k--) {
                heat_0[k] = (heat_0[k - 1] + heat_0[k - 2] + heat_0[k - 2]) / 3;
                heat_1[k] = (heat_1[k - 1] + heat_1[k - 2] + heat_1[k - 2]) / 3;
            }
        }

        EVERY_N_MILLISECONDS(100) {
            // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
            if (random8() < SPARKING) {
                int y = random8(4);
                heat_0[y] = qadd8(heat_0[y], random8(50, 100));
            }
            if (random8() < SPARKING) {
                int y = random8(4);
                heat_1[y] = qadd8(heat_1[y], random8(50, 100));
            }
        }

        CRGBPalette16 gPal;

        //  gPal = HeatColors_p;
        // These are other ways to set up the color palette for the 'fire'.
        // First, a gradient from black to red to yellow to white -- similar to HeatColors_p
        gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Orange, CRGB::Yellow);

        // Second, this palette is like the heat colors, but blue/aqua instead of red/yellow
        // gPal = CRGBPalette16(CRGB::Black, CRGB::Blue, CRGB(35, 255, 255));

        // Third, here's a simpler, three-step gradient, from black to red to white
        //  gPal = CRGBPalette16( CRGB::Black, CRGB::Green, CRGB::Cyan, CRGB(50, 255, 255));

        //  EVERY_N_MILLISECONDS(500) {
        //    static byte colorindex = 0;
        //    for ( int j = 0; j < NUM_LEDS; j++) {
        //      leds_0[j] = leds_1[j] = ColorFromPalette(gPal, colorindex + j);
        //    }
        //    colorindex += 8;
        ////    if (colorindex > 240)
        ////      colorindex = 0;
        //  }

        // Step 4.  Map from heat cells to LED colors
        for (int j = 0; j < NUM_LEDS / 2; j++) {
            // Scale the heat value from 0-255 down to 0-240
            // for best results with color palettes.
            byte scale_limit = 255;
            byte colorindex = scale8(heat_0[j], scale_limit);
            CRGB color = ColorFromPalette(gPal, colorindex);
            _leds[j] = color;

            colorindex = scale8(heat_1[j], scale_limit);
            color = ColorFromPalette(gPal, colorindex);
            _leds[NUM_LEDS - 1 - j] = color;
        }
    }
};

/*---------------------------------------------------------------------------*/

#endif
