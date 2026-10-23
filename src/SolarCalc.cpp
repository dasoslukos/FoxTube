#include "SolarCalc.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace SolarCalc {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double RAD = PI / 180.0;
const char * const ZONE_NAMES[] = {
  "US Eastern (Athens)", "US Central", "US Mountain", "US Pacific",
  "Arizona (no DST)", "UTC", "UK", "Europe Central",
  "Australia Eastern", "New Zealand", "Custom POSIX TZ"
};
const char * const ZONE_RULES[] = {
  "EST5EDT,M3.2.0/2,M11.1.0/2",
  "CST6CDT,M3.2.0/2,M11.1.0/2",
  "MST7MDT,M3.2.0/2,M11.1.0/2",
  "PST8PDT,M3.2.0/2,M11.1.0/2",
  "MST7", "UTC0",
  "GMT0BST,M3.5.0/1,M10.5.0/2",
  "CET-1CEST,M3.5.0/2,M10.5.0/3",
  "AEST-10AEDT,M10.1.0/2,M4.1.0/3",
  "NZST-12NZDT,M9.5.0/2,M4.1.0/3",
  nullptr
};

double norm(double angle) {
  angle = fmod(angle, 360.0);
  return angle < 0 ? angle + 360.0 : angle;
}

// Days since Unix epoch, independent of the configured local timezone.
// Howard Hinnant's civil-date to days algorithm (Gregorian calendar).
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const int mp = static_cast<int>(m) + (m > 2 ? -3 : 9);
  const unsigned doy = (153 * static_cast<unsigned>(mp) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<int64_t>(era) * 146097 + static_cast<int>(doe) - 719468;
}

// NOAA/USNO simplified solar crossing algorithm. Returns unnormalised UT hours.
// Keeping negative/over-24 values allows global longitudes to resolve the
// *correct local date*, rather than silently switching to yesterday.
bool utcHour(int year, int month, int day, double lat, double lon,
             bool sunrise, double &hours) {
  const double n = double(daysFromCivil(year, month, day) -
                          daysFromCivil(year, 1, 1) + 1);
  const double lngHour = lon / 15.0;
  const double t = n + ((sunrise ? 6.0 : 18.0) - lngHour) / 24.0;
  const double M = 0.9856 * t - 3.289;
  const double L = norm(M + 1.916 * sin(M * RAD) +
                        0.020 * sin(2 * M * RAD) + 282.634);
  double RA = norm(atan(0.91764 * tan(L * RAD)) / RAD);
  RA += floor(L / 90.0) * 90.0 - floor(RA / 90.0) * 90.0;
  RA /= 15.0;
  const double sinDec = 0.39782 * sin(L * RAD);
  const double cosDec = cos(asin(sinDec));
  const double cosH = (cos(90.833 * RAD) - sinDec * sin(lat * RAD)) /
                      (cosDec * cos(lat * RAD));
  if (!isfinite(cosH) || cosH > 1.0 || cosH < -1.0)
    return false; // polar-day/night: caller uses manual fallback
  const double angle = acos(cosH) / RAD;
  const double H = (sunrise ? 360.0 - angle : angle) / 15.0;
  const double T = H + RA - 0.06571 * t - 6.622;
  hours = T - lngHour;
  return isfinite(hours);
}

bool eventOnLocalDate(int year, int month, int day, double lat, double lon,
                      bool rise, int64_t &epoch, uint16_t &minutes) {
  double raw = 0;
  if (!utcHour(year, month, day, lat, lon, rise, raw)) return false;
  const int64_t base = daysFromCivil(year, month, day) * 86400;
  // The analytic expression has a 24h ambiguity. Find the unique candidate
  // whose timezone-converted civil date equals the requested local date.
  for (int offset = -2; offset <= 2; ++offset) {
    const int64_t candidate = base + static_cast<int64_t>(llround((raw + offset * 24.0) * 3600.0));
    const time_t t = static_cast<time_t>(candidate);
    struct tm local = {};
    if (localtime_r(&t, &local) && local.tm_year + 1900 == year &&
        local.tm_mon + 1 == month && local.tm_mday == day) {
      epoch = candidate;
      minutes = static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min);
      return true;
    }
  }
  return false;
}
} // namespace

const char *zoneName(Zone zone) {
  return static_cast<unsigned>(zone) < ZONE_COUNT ? ZONE_NAMES[zone] : ZONE_NAMES[0];
}
const char *zoneRule(Zone zone) {
  return static_cast<unsigned>(zone) < CUSTOM ? ZONE_RULES[zone] : nullptr;
}

bool validCustomRule(const char *rule) {
  if (!rule) return false;
  size_t n = strlen(rule);
  if (n < 4 || n > 63) return false;
  if (!((rule[0] >= 'A' && rule[0] <= 'Z') ||
        (rule[0] >= 'a' && rule[0] <= 'z'))) return false;
  bool digitFound = false;
  for (size_t i=0; i<n; ++i) {
    char c = rule[i];
    if (c >= '0' && c <= '9') digitFound = true;
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '+' ||
          c == ',' || c == '.' || c == '/' || c == ':')) return false;
  }
  return digitFound;
}

bool configureZone(Zone zone, const char *customRule) {
  const char *tz = zoneRule(zone);
  if (zone == CUSTOM) {
    if (!validCustomRule(customRule)) return false;
    tz = customRule;
  }
  if (!tz) return false;
  if (setenv("TZ", tz, 1) != 0) return false;
  tzset();
  return true;
}

Events calculateForDate(int year, int month, int day, double lat, double lon) {
  Events e = {};
  e.valid = false;
  if (!isfinite(lat) || !isfinite(lon) || fabs(lat) > 89.9 || fabs(lon) > 180.0)
    return e;
  if (!eventOnLocalDate(year, month, day, lat, lon, true,
                        e.sunriseUTC, e.sunriseMinutes)) return e;
  if (!eventOnLocalDate(year, month, day, lat, lon, false,
                        e.sunsetUTC, e.sunsetMinutes)) return e;
  e.valid = e.sunriseUTC < e.sunsetUTC;
  return e;
}
} // namespace SolarCalc
