#ifndef FOXTube_DISPLAY_SCHEDULE_H
#define FOXTube_DISPLAY_SCHEDULE_H

#include "GLOBAL_DEFINES.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <Preferences.h>
#include <stdint.h>

class TFTs;

class DisplaySchedule
{
public:
  DisplaySchedule();

  void begin(TFTs *tfts_);
  void loop(uint8_t hour, uint8_t minute);
  void applyNow(uint8_t hour, uint8_t minute);

  bool getEnabled() const { return enabled; }
  uint16_t getDayStartMinutes() const { return day_start_minutes; }
  uint16_t getNightStartMinutes() const { return night_start_minutes; }
  uint8_t getDayBrightness() const { return day_brightness; }
  uint8_t getNightBrightness() const { return night_brightness; }
  uint8_t getAppliedBrightness() const { return applied_brightness; }

  bool isNight(uint8_t hour, uint8_t minute) const;

  void setEnabled(bool value);
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
  uint16_t day_start_minutes;
  uint16_t night_start_minutes;
  uint8_t day_brightness;
  uint8_t night_brightness;
  uint16_t last_checked_minute;
  uint8_t applied_brightness;

  void load();
  void save();
  uint8_t targetBrightness(uint8_t hour, uint8_t minute) const;
  static uint16_t wrapMinutes(int32_t minutes);
};

#endif // HARDWARE_IPSTUBE_S3_CLOCK

#endif // FOXTube_DISPLAY_SCHEDULE_H
