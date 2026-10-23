#include "DisplaySchedule.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <Arduino.h>
#include <math.h>
#include <time.h>
#include "Clock.h"
#include "TFTs.h"

namespace {
constexpr int32_t ATHENS_LAT_E6 = 33951900;
constexpr int32_t ATHENS_LON_E6 = -83357600;
constexpr time_t MIN_VALID_UTC = 1761609600; // NTP validity used by FoxTube
}

DisplaySchedule::DisplaySchedule()
    : tfts(nullptr), started(false), enabled(true), solar_mode(false),
      day_start_minutes(7 * 60), night_start_minutes(22 * 60),
      day_brightness(255), night_brightness(20),
      latitude_e6(ATHENS_LAT_E6), longitude_e6(ATHENS_LON_E6),
      zone(SolarCalc::US_EASTERN), custom_tz("UTC0"),
      sunrise_offset(0), sunset_offset(0), solar_day_key(-1),
      solar_events({}), last_checked_minute(UINT32_MAX), applied_brightness(255)
{
}

void DisplaySchedule::begin(TFTs *tfts_)
{
  tfts = tfts_;
  prefs.begin("foxdisplay", false);
  load();
  started = true;
  Serial.printf("FoxTube display: enabled=%u solar=%u zone=%s position=%.5f,%.5f\n",
                enabled, solar_mode, SolarCalc::zoneName(static_cast<SolarCalc::Zone>(zone)),
                getLatitude(), getLongitude());
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
    solar_mode = false;
    save();
  }
  else
  {
    enabled = prefs.getBool("en", true);
    day_start_minutes = prefs.getUShort("daymin", 7 * 60) % minutes_per_day;
    night_start_minutes = prefs.getUShort("nightmin", 22 * 60) % minutes_per_day;
    day_brightness = prefs.getUChar("daybri", 255);
    night_brightness = prefs.getUChar("nightbri", 20);
    solar_mode = prefs.getBool("solar", false); // preserve existing manual settings on upgrade
  }
  latitude_e6 = prefs.getInt("latE6", ATHENS_LAT_E6);
  longitude_e6 = prefs.getInt("lonE6", ATHENS_LON_E6);
  if (latitude_e6 < -89900000 || latitude_e6 > 89900000 ||
      longitude_e6 < -180000000 || longitude_e6 > 180000000) {
    latitude_e6 = ATHENS_LAT_E6;
    longitude_e6 = ATHENS_LON_E6;
  }
  zone = prefs.getUChar("tzID", SolarCalc::US_EASTERN);
  if (zone >= SolarCalc::ZONE_COUNT) zone = SolarCalc::US_EASTERN;
  custom_tz = prefs.getString("tzRule", "UTC0");
  if (!SolarCalc::validCustomRule(custom_tz.c_str())) custom_tz = "UTC0";
  if (!SolarCalc::configureZone(static_cast<SolarCalc::Zone>(zone), custom_tz.c_str())) {
    zone = SolarCalc::US_EASTERN;
    SolarCalc::configureZone(SolarCalc::US_EASTERN, nullptr);
  }
  sunrise_offset = constrain(int(prefs.getShort("riseOff", 0)), -120, 120);
  sunset_offset = constrain(int(prefs.getShort("setOff", 0)), -120, 120);
  invalidateSolar();
}

void DisplaySchedule::save()
{
  prefs.putUChar("valid", valid_marker);
  prefs.putBool("en", enabled);
  prefs.putUShort("daymin", day_start_minutes);
  prefs.putUShort("nightmin", night_start_minutes);
  prefs.putUChar("daybri", day_brightness);
  prefs.putUChar("nightbri", night_brightness);
  prefs.putBool("solar", solar_mode);
  prefs.putInt("latE6", latitude_e6);
  prefs.putInt("lonE6", longitude_e6);
  prefs.putUChar("tzID", zone);
  prefs.putString("tzRule", custom_tz);
  prefs.putShort("riseOff", sunrise_offset);
  prefs.putShort("setOff", sunset_offset);
}

void DisplaySchedule::invalidateSolar()
{
  solar_day_key = -1;
  solar_events = {};
  last_checked_minute = UINT32_MAX;
}

bool DisplaySchedule::ensureSolar() const
{
  if (!solar_mode || uclock.loop_time < MIN_VALID_UTC) return false;
  const time_t utc_now = static_cast<time_t>(uclock.loop_time);
  struct tm local = {};
  if (!localtime_r(&utc_now, &local)) return false;
  const int32_t key = (local.tm_year + 1900) * 1000 + local.tm_yday;
  if (key != solar_day_key) {
    solar_day_key = key;
    solar_events = SolarCalc::calculateForDate(local.tm_year + 1900,
        local.tm_mon + 1, local.tm_mday, getLatitude(), getLongitude());
    if (solar_events.valid)
      Serial.printf("FoxTube solar: sunrise %02u:%02u, sunset %02u:%02u, %s\n",
          solar_events.sunriseMinutes / 60, solar_events.sunriseMinutes % 60,
          solar_events.sunsetMinutes / 60, solar_events.sunsetMinutes % 60,
          SolarCalc::zoneName(static_cast<SolarCalc::Zone>(zone)));
    else
      Serial.println("FoxTube solar: no sunrise/sunset for this date; using manual schedule");
  }
  return solar_events.valid &&
         (solar_events.sunriseUTC + int64_t(sunrise_offset) * 60) <
         (solar_events.sunsetUTC + int64_t(sunset_offset) * 60);
}

bool DisplaySchedule::solarValid() const { return ensureSolar(); }
uint16_t DisplaySchedule::sunriseMinutes() const { return ensureSolar() ? solar_events.sunriseMinutes : 0; }
uint16_t DisplaySchedule::sunsetMinutes() const { return ensureSolar() ? solar_events.sunsetMinutes : 0; }

uint16_t DisplaySchedule::wrapMinutes(int32_t minutes)
{
  minutes %= minutes_per_day;
  if (minutes < 0) minutes += minutes_per_day;
  return uint16_t(minutes);
}

bool DisplaySchedule::manualNight(uint8_t hour, uint8_t minute) const
{
  const uint16_t now = (uint16_t(hour % 24) * 60) + (minute % 60);
  if (day_start_minutes == night_start_minutes) return false;
  const bool day_now = day_start_minutes < night_start_minutes
      ? now >= day_start_minutes && now < night_start_minutes
      : now >= day_start_minutes || now < night_start_minutes;
  return !day_now;
}

bool DisplaySchedule::isNight(uint8_t hour, uint8_t minute) const
{
  if (!enabled) return false;
  if (ensureSolar()) {
    const int64_t now = static_cast<int64_t>(uclock.loop_time);
    const int64_t sunrise = solar_events.sunriseUTC + int64_t(sunrise_offset) * 60;
    const int64_t sunset = solar_events.sunsetUTC + int64_t(sunset_offset) * 60;
    return now < sunrise || now >= sunset;
  }
  return manualNight(hour, minute);
}

uint8_t DisplaySchedule::targetBrightness(uint8_t hour, uint8_t minute) const
{
  return isNight(hour, minute) ? night_brightness : day_brightness;
}

void DisplaySchedule::loop(uint8_t hour, uint8_t minute)
{
  if (!started || tfts == nullptr) return;
  // UTC minute key handles a changed date, NTP corrections and DST crossings.
  const uint32_t key = uclock.loop_time >= MIN_VALID_UTC
      ? uint32_t(uclock.loop_time / 60) : uint32_t(hour * 60 + minute);
  if (key == last_checked_minute) return;
  last_checked_minute = key;
  const uint8_t target = targetBrightness(hour, minute);
  if (target == applied_brightness && tfts->dimming == target) return;
  applied_brightness = target;
  tfts->dimming = target;
  tfts->ProcessUpdatedDimming();
  Serial.printf("FoxTube display schedule: %s, brightness=%u\n",
                isNight(hour, minute) ? "night" : "day", target);
}

void DisplaySchedule::applyNow(uint8_t hour, uint8_t minute)
{
  if (!started || tfts == nullptr) return;
  last_checked_minute = UINT32_MAX;
  applied_brightness = targetBrightness(hour, minute);
  tfts->dimming = applied_brightness;
  tfts->ProcessUpdatedDimming();
  last_checked_minute = uclock.loop_time >= MIN_VALID_UTC
      ? uint32_t(uclock.loop_time / 60) : uint32_t(hour * 60 + minute);
  Serial.printf("FoxTube display schedule applied: %s, brightness=%u\n",
                isNight(hour, minute) ? "night" : "day", applied_brightness);
}

void DisplaySchedule::setEnabled(bool value) { if (enabled != value) { enabled = value; save(); } }
void DisplaySchedule::setSolarMode(bool value) {
  if (solar_mode != value) { solar_mode = value; invalidateSolar(); save(); }
}
bool DisplaySchedule::setLocation(double latitude, double longitude) {
  if (!isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 89.9 || fabs(longitude) > 180.0)
    return false;
  const int32_t lat = static_cast<int32_t>(lround(latitude * 1000000));
  const int32_t lon = static_cast<int32_t>(lround(longitude * 1000000));
  if (lat != latitude_e6 || lon != longitude_e6) {
    latitude_e6 = lat; longitude_e6 = lon; invalidateSolar(); save();
  }
  return true;
}
bool DisplaySchedule::setZone(uint8_t zoneId, const String &customTZ) {
  if (zoneId >= SolarCalc::ZONE_COUNT) return false;
  const String selectedTZ = zoneId == SolarCalc::CUSTOM ? customTZ : custom_tz;
  if (!SolarCalc::configureZone(static_cast<SolarCalc::Zone>(zoneId), selectedTZ.c_str())) return false;
  if (zone != zoneId || custom_tz != selectedTZ) {
    zone = zoneId; custom_tz = selectedTZ; invalidateSolar(); save();
  }
  return true;
}
void DisplaySchedule::setSunriseOffset(int16_t minutes) {
  int16_t next = constrain(int(minutes), -120, 120);
  if (next != sunrise_offset) { sunrise_offset = next; invalidateSolar(); save(); }
}
void DisplaySchedule::setSunsetOffset(int16_t minutes) {
  int16_t next = constrain(int(minutes), -120, 120);
  if (next != sunset_offset) { sunset_offset = next; invalidateSolar(); save(); }
}
void DisplaySchedule::resetAthens() {
  latitude_e6 = ATHENS_LAT_E6; longitude_e6 = ATHENS_LON_E6;
  zone = SolarCalc::US_EASTERN;
  SolarCalc::configureZone(SolarCalc::US_EASTERN, nullptr);
  sunrise_offset = sunset_offset = 0;
  invalidateSolar(); save();
}
void DisplaySchedule::setDayStartMinutes(uint16_t minutes) { day_start_minutes = minutes % minutes_per_day; save(); }
void DisplaySchedule::setNightStartMinutes(uint16_t minutes) { night_start_minutes = minutes % minutes_per_day; save(); }
void DisplaySchedule::setDayBrightness(uint8_t brightness) { day_brightness = brightness; save(); }
void DisplaySchedule::setNightBrightness(uint8_t brightness) { night_brightness = brightness; save(); }
void DisplaySchedule::adjustDayStartMinutes(int16_t delta_minutes) {
  day_start_minutes = wrapMinutes(int32_t(day_start_minutes) + delta_minutes); save();
}
void DisplaySchedule::adjustNightStartMinutes(int16_t delta_minutes) {
  night_start_minutes = wrapMinutes(int32_t(night_start_minutes) + delta_minutes); save();
}
void DisplaySchedule::adjustDayBrightness(int16_t delta) {
  if (delta > 0)
    day_brightness = day_brightness >= 240 ? (day_brightness == 255 ? 0 : 255)
                                           : uint8_t(day_brightness + delta);
  else if (delta < 0)
    day_brightness = day_brightness <= uint8_t(-delta) ? (day_brightness == 0 ? 255 : 0)
                                                       : uint8_t(day_brightness + delta);
  save();
}
void DisplaySchedule::adjustNightBrightness(int16_t delta) {
  if (delta > 0)
    night_brightness = night_brightness >= 240 ? (night_brightness == 255 ? 0 : 255)
                                               : uint8_t(night_brightness + delta);
  else if (delta < 0)
    night_brightness = night_brightness <= uint8_t(-delta) ? (night_brightness == 0 ? 255 : 0)
                                                         : uint8_t(night_brightness + delta);
  save();
}

#endif // HARDWARE_IPSTUBE_S3_CLOCK
