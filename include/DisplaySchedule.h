#ifndef FOXTube_DISPLAY_SCHEDULE_H
#define FOXTube_DISPLAY_SCHEDULE_H

#include "GLOBAL_DEFINES.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <Arduino.h>
#include <Preferences.h>
#include <stdint.h>
#include "SolarCalc.h"

class TFTs;

class DisplaySchedule
{
public:
  DisplaySchedule();
  void begin(TFTs *tfts_);
  void loop(uint8_t hour, uint8_t minute);
  void applyNow(uint8_t hour, uint8_t minute);

  bool getEnabled() const { return enabled; }
  bool getSolarMode() const { return solar_mode; }
  uint16_t getDayStartMinutes() const { return day_start_minutes; }
  uint16_t getNightStartMinutes() const { return night_start_minutes; }
  uint8_t getDayBrightness() const { return day_brightness; }
  uint8_t getNightBrightness() const { return night_brightness; }
  uint8_t getAppliedBrightness() const { return applied_brightness; }
  double getLatitude() const { return latitude_e6 / 1000000.0; }
  double getLongitude() const { return longitude_e6 / 1000000.0; }
  uint8_t getZone() const { return zone; }
  const String &getCustomTZ() const { return custom_tz; }
  int16_t getSunriseOffset() const { return sunrise_offset; }
  int16_t getSunsetOffset() const { return sunset_offset; }
  bool solarValid() const;
  uint16_t sunriseMinutes() const;
  uint16_t sunsetMinutes() const;
  bool isNight(uint8_t hour, uint8_t minute) const;

  void setEnabled(bool value);
  void setSolarMode(bool value);
  bool setLocation(double latitude, double longitude);
  bool setZone(uint8_t zoneId, const String &customTZ);
  void setSunriseOffset(int16_t minutes);
  void setSunsetOffset(int16_t minutes);
  void resetAthens();
  void setDayStartMinutes(uint16_t minutes);
  void setNightStartMinutes(uint16_t minutes);
  void setDayBrightness(uint8_t brightness);
  void setNightBrightness(uint8_t brightness);
  void adjustDayStartMinutes(int16_t delta_minutes);
  void adjustNightStartMinutes(int16_t delta_minutes);
  void adjustDayBrightness(int16_t delta);
  void adjustNightBrightness(int16_t delta);

private:
  static const uint8_t valid_marker = 0x55;
  static const uint16_t minutes_per_day = 24 * 60;

  Preferences prefs;
  TFTs *tfts;
  bool started;
  bool enabled;
  bool solar_mode;
  uint16_t day_start_minutes;
  uint16_t night_start_minutes;
  uint8_t day_brightness;
  uint8_t night_brightness;
  int32_t latitude_e6;
  int32_t longitude_e6;
  uint8_t zone;
  String custom_tz;
  int16_t sunrise_offset;
  int16_t sunset_offset;
  mutable int32_t solar_day_key;
  mutable SolarCalc::Events solar_events;
  uint32_t last_checked_minute;
  uint8_t applied_brightness;

  void load();
  void save();
  void invalidateSolar();
  bool ensureSolar() const;
  bool manualNight(uint8_t hour, uint8_t minute) const;
  uint8_t targetBrightness(uint8_t hour, uint8_t minute) const;
  static uint16_t wrapMinutes(int32_t minutes);
};

#endif // HARDWARE_IPSTUBE_S3_CLOCK
#endif // FOXTube_DISPLAY_SCHEDULE_H
