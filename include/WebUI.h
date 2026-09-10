#ifndef FOXTube_WEBUI_H
#define FOXTube_WEBUI_H

#include "GLOBAL_DEFINES.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <WebServer.h>

class Backlights;
class Clock;
class StoredConfig;
class TFTs;

class WebUI
{
public:
  WebUI();

  void begin(Backlights *backlights_, TFTs *tfts_, Clock *clock_, StoredConfig *stored_config_);
  void loop();

private:
  WebServer server;
  Backlights *backlights;
  TFTs *tfts;
  Clock *clock;
  StoredConfig *stored_config;
  bool started;
  bool mdns_started;

  void installRoutes();
  void handleRoot();
  void handleState();
  void handleTube();
  void handleStrip();
  void handleClock();
  void handlePanorama();
  void handleNotFound();

  void sendOk();
  void redrawClock();
  uint16_t htmlColorToPhase(const String &value);
  String colorToHtml(uint32_t color);
  String jsonEscape(const String &value);
};

#endif // HARDWARE_IPSTUBE_S3_CLOCK

#endif // FOXTube_WEBUI_H
