
#ifndef nursery_web_server_h
#define nursery_web_server_h

#include "led_strip_controller.h"
#include "nursery_monitor.h"
#include <FS.h>
#include <WebServer.h>

/*---------------------------------------------------------------------------*/

/**
 * Presents HTTP endpoints for controlling the NurseryServer.
 */
class NurseryWebServer {
    LEDStripController& _strip_controller;
    LEDRing& _led_ring;
    fs::FS& _fs;
    NurseryMonitor& _monitor;
    WebServer _server;

public:
    NurseryWebServer(LEDStripController& strip_controller, LEDRing& led_ring, fs::FS& fs, NurseryMonitor& monitor)
        : _strip_controller(strip_controller)
        , _led_ring(led_ring)
        , _fs(fs)
        , _monitor(monitor)
        , _server(80)
    {
        _server.on("/", [this]() { this->handle_root(); });
        _server.on("/brighter", [this]() { this->handle_brighter(); });
        _server.on("/dimmer", [this]() { this->handle_dimmer(); });
        _server.on("/off", [this]() { this->handle_off(); });
        _server.on("/status", [this]() { this->handle_status(); });
        _server.on("/timeout", [this]() { this->handle_timeout(); });
        _server.on("/wake", [this]() { this->handle_wake(); });
        _server.onNotFound([this]() { this->handleNotFound(); });
    }

    void begin() { _server.begin(); }
    void handleClient() { _server.handleClient(); }

private:
    void handle_root()
    {
        _server.sendHeader("Location", "/index.html", true);
        _server.send(308, "text/plain", "");
    }

    void handle_brighter()
    {
        _strip_controller.increase_brightness();
        _server.send(200, "text/plain", "OK");
    }

    void handle_dimmer()
    {
        _strip_controller.decrease_brightness();
        _server.send(200, "text/plain", "OK");
    }

    void handle_off()
    {
        _strip_controller.turn_off();
        _led_ring.setMode(LEDRing::OFF);
        _server.send(200, "text/plain", "OK");
    }

    void handle_wake()
    {
        _strip_controller.begin_wake();
        _server.send(200, "text/plain", "OK");
    }

    void handle_timeout()
    {
        if (_led_ring.mode() != LEDRing::TIMEOUT)
            _led_ring.setMode(LEDRing::TIMEOUT);
        else
            _led_ring.setMode(LEDRing::OFF);
        _server.send(200, "text/plain", "OK");
    }

    void handleNotFound()
    {
        String uri = _server.uri();
        File file = _fs.open(uri, "r");
        if (!file || file.isDirectory()) {
            _server.send(404, "text/plain", "File not found");
        } else {
            String content_type = "application/octet-stream";
            if (uri.endsWith(".html"))
                content_type = "text/html";
            else if (uri.endsWith(".css"))
                content_type = "text/css";
            _server.streamFile(file, content_type);
            file.close();
        }
    }

    void handle_status()
    {
        StaticJsonDocument<1024> doc;

        _strip_controller.add_status(doc);
        _monitor.add_status(doc);

        String json;
        serializeJson(doc, json);
        _server.send(200, "text/json", json);
    }
};

/*---------------------------------------------------------------------------*/

#endif
