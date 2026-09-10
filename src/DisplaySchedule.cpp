#include "DisplaySchedule.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <Arduino.h>
#include "TFTs.h"

DisplaySchedule::DisplaySchedule()
    : tfts(nullptr), started(false), enabled(true),
      day_start_minutes(7 * 60), night_start_minutes(22 * 60),
      day_brightness(255), night_brightness(20),
      last_checked_minute(0xFFFF), applied_brightness(255)
{
}

void DisplaySchedule::begin(TFTs *tfts_)
{
  tfts = tfts_;
  prefs.begin("foxdisplay", false);
  load();
  started = true;

  Serial.println("FoxTube display schedule loaded:");
  Serial.printf("  enabled=%s\n", enabled ? "yes" : "no");
  Serial.printf("  day=%02u:%02u brightness=%u\n",
                day_start_minutes / 60, day_start_minutes % 60, day_brightness);
  Serial.printf("  night=%02u:%02u brightness=%u\n",
                night_start_minutes / 60, night_start_minutes % 60, night_brightness);
}

void DisplaySchedule::load()
{
  if (prefs.getUChar("valid", 0) != valid_marker)
  {
#ifdef DAY_TIME
    day_start_minutes = (uint16_t(DAY_TIME) % 24) * 60;
#else
    day_start_minutes = 7 * 60;
#endif

#ifdef NIGHT_TIME
    night_start_minutes = (uint16_t(NIGHT_TIME) % 24) * 60;
#else
    night_start_minutes = 22 * 60;
#endif

    day_brightness = 255;
#ifdef TFT_DIMMED_INTENSITY
    night_brightness = uint8_t(TFT_DIMMED_INTENSITY);
#else
    night_brightness = 20;
#endif
    enabled = true;
    save();
    return;
  }

  enabled = prefs.getBool("en", true);
  day_start_minutes = prefs.getUShort("daymin", 7 * 60) % minutes_per_day;
  night_start_minutes = prefs.getUShort("nightmin", 22 * 60) % minutes_per_day;
  day_brightness = prefs.getUChar("daybri", 255);
  night_brightness = prefs.getUChar("nightbri", 20);
}

void DisplaySchedule::save()
{
  prefs.putUChar("valid", valid_marker);
  prefs.putBool("en", enabled);
  prefs.putUShort("daymin", day_start_minutes);
  prefs.putUShort("nightmin", night_start_minutes);
  prefs.putUChar("daybri", day_brightness);
  prefs.putUChar("nightbri", night_brightness);
}

uint16_t DisplaySchedule::wrapMinutes(int32_t minutes)
{
  minutes %= minutes_per_day;
  if (minutes < 0)
    minutes += minutes_per_day;
  return uint16_t(minutes);
}

bool DisplaySchedule::isNight(uint8_t hour, uint8_t minute) const
{
  if (!enabled)
    return false;

  const uint16_t now = (uint16_t(hour % 24) * 60) + (minute % 60);

  // If both boundaries are equal, treat the clock as always in day mode.
  if (day_start_minutes == night_start_minutes)
    return false;

  // Day is the interval beginning at day_start_minutes and ending at
  // night_start_minutes. This works whether or not the day interval crosses
  // midnight; night is simply everything outside that interval.
  bool day_now;
  if (day_start_minutes < night_start_minutes)
    day_now = now >= day_start_minutes && now < night_start_minutes;
  else
    day_now = now >= day_start_minutes || now < night_start_minutes;

  return !day_now;
}

uint8_t DisplaySchedule::targetBrightness(uint8_t hour, uint8_t minute) const
{
  return isNight(hour, minute) ? night_brightness : day_brightness;
}

void DisplaySchedule::loop(uint8_t hour, uint8_t minute)
{
  if (!started || tfts == nullptr)
    return;

  const uint16_t now = (uint16_t(hour % 24) * 60) + (minute % 60);
  if (now == last_checked_minute)
    return;

  last_checked_minute = now;
  const uint8_t target = targetBrightness(hour, minute);
  if (target == applied_brightness && tfts->dimming == target)
    return;

  applied_brightness = target;
  tfts->dimming = target;
  tfts->ProcessUpdatedDimming();

  Serial.printf("FoxTube display schedule: %s mode, brightness=%u\n",
                isNight(hour, minute) ? "night" : "day", target);
}

void DisplaySchedule::applyNow(uint8_t hour, uint8_t minute)
{
  if (!started || tfts == nullptr)
    return;

  last_checked_minute = 0xFFFF;
  applied_brightness = targetBrightness(hour, minute);
  tfts->dimming = applied_brightness;
  tfts->ProcessUpdatedDimming();
  last_checked_minute = (uint16_t(hour % 24) * 60) + (minute % 60);

  Serial.printf("FoxTube display schedule applied: %s mode, brightness=%u\n",
                isNight(hour, minute) ? "night" : "day", applied_brightness);
}

void DisplaySchedule::setEnabled(bool value)
{
  enabled = value;
  save();
}

void DisplaySchedule::setDayStartMinutes(uint16_t minutes)
{
  day_start_minutes = minutes % minutes_per_day;
  save();
}

void DisplaySchedule::setNightStartMinutes(uint16_t minutes)
{
  night_start_minutes = minutes % minutes_per_day;
  save();
}

void DisplaySchedule::setDayBrightness(uint8_t brightness)
{
  day_brightness = brightness;
  save();
}

void DisplaySchedule::setNightBrightness(uint8_t brightness)
{
  night_brightness = brightness;
  save();
}

void DisplaySchedule::adjustDayStartMinutes(int16_t delta_minutes)
{
  day_start_minutes = wrapMinutes(int32_t(day_start_minutes) + delta_minutes);
  save();
}

void DisplaySchedule::adjustNightStartMinutes(int16_t delta_minutes)
{
  night_start_minutes = wrapMinutes(int32_t(night_start_minutes) + delta_minutes);
  save();
}

void DisplaySchedule::adjustDayBrightness(int16_t delta)
{
  if (delta > 0)
    day_brightness = day_brightness >= 240 ? (day_brightness == 255 ? 0 : 255)
                                           : uint8_t(day_brightness + delta);
  else if (delta < 0)
    day_brightness = day_brightness <= uint8_t(-delta) ? (day_brightness == 0 ? 255 : 0)
                                                        : uint8_t(day_brightness + delta);
  save();
}

void DisplaySchedule::adjustNightBrightness(int16_t delta)
{
  if (delta > 0)
    night_brightness = night_brightness >= 240 ? (night_brightness == 255 ? 0 : 255)
                                               : uint8_t(night_brightness + delta);
  else if (delta < 0)
    night_brightness = night_brightness <= uint8_t(-delta) ? (night_brightness == 0 ? 255 : 0)
                                                            : uint8_t(night_brightness + delta);
  save();
}

#endif // HARDWARE_IPSTUBE_S3_CLOCK
