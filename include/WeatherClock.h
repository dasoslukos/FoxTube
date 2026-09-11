#ifndef FOXTube_WEATHER_CLOCK_H
#define FOXTube_WEATHER_CLOCK_H

#include "GLOBAL_DEFINES.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <Arduino.h>
#include <Preferences.h>

class Clock;
class TFTs;

class WeatherClock
{
public:
  enum mode_t : uint8_t
  {
    normal_mode = 0,
    weather_mode = 1
  };

  static const uint8_t MAX_STATIONS = 6;

  struct StationInfo
  {
    String mac;
    String name;
  };

  WeatherClock();

  void begin(TFTs *tfts_, Clock *clock_);
  void loop();

  mode_t getMode() const { return mode; }
  bool isWeatherMode() const { return mode == weather_mode; }
  void setMode(mode_t new_mode);
  void toggleMode();

  bool credentialsConfigured() const;
  bool saveCredentials(const String &api_key, const String &application_key);
  void clearCredentials();

  bool fetchNow();
  void requestRefresh();

  void setRefreshMinutes(uint16_t minutes);
  uint16_t getRefreshMinutes() const { return refresh_minutes; }

  void setStationMac(const String &mac);
  String getStationMac() const { return station_mac; }
  String getStationName() const { return selected_station_name; }

  uint8_t getStationCount() const { return station_count; }
  const StationInfo &getStation(uint8_t index) const { return stations[index]; }

  bool hasReading() const { return reading_valid; }
  float getTemperatureF() const { return temp_f; }
  float getHumidity() const { return humidity; }
  float getWindMph() const { return wind_mph; }
  float getDailyRainIn() const { return daily_rain_in; }

  uint32_t getLastSuccessAgeSeconds() const;
  bool isStale() const;
  String getLastError() const { return last_error; }

  void render(bool force = false);

private:
  Preferences prefs;
  TFTs *tfts;
  Clock *clock;

  mode_t mode;

  String api_key;
  String application_key;
  String station_mac;
  String selected_station_name;

  uint16_t refresh_minutes;
  bool refresh_requested;
  uint32_t last_attempt_millis;
  uint32_t last_success_millis;

  bool reading_valid;
  float temp_f;
  float humidity;
  float wind_mph;
  float daily_rain_in;
  uint64_t station_date_utc_ms;

  StationInfo stations[MAX_STATIONS];
  uint8_t station_count;

  String last_error;

  uint8_t last_render_hour;
  uint8_t last_render_minute;
  uint32_t reading_generation;
  uint32_t rendered_generation;

  void loadConfig();
  void saveConfig();
  void clearStationList();
  bool parseDevicesJson(const String &payload);
  int findSelectedStation() const;
};

extern WeatherClock weather_clock;

#endif // HARDWARE_IPSTUBE_S3_CLOCK

#endif // FOXTube_WEATHER_CLOCK_H
