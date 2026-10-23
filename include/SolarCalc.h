#ifndef FOX_TUBE_SOLAR_CALC_H
#define FOX_TUBE_SOLAR_CALC_H

#include <stdint.h>
#include <time.h>

// Calculates official sunrise/sunset (solar centre 90.833 degrees below zenith)
// for the selected LOCAL civil date. POSIX timezone determines DST on that date.
// No network I/O and no heap allocations in the numerical calculation.
namespace SolarCalc {
  // Named location time zones. 'custom' uses a user-supplied POSIX TZ rule.
  enum Zone : uint8_t {
    US_EASTERN = 0, US_CENTRAL, US_MOUNTAIN, US_PACIFIC,
    US_ARIZONA, UTC, UK, EU_CENTRAL, AU_EASTERN, NZ,
    CUSTOM, ZONE_COUNT
  };
  const char *zoneName(Zone zone);
  const char *zoneRule(Zone zone);
  bool validCustomRule(const char *rule);
  bool configureZone(Zone zone, const char *customRule);

  struct Events {
    bool valid;
    int64_t sunriseUTC;
    int64_t sunsetUTC;
    uint16_t sunriseMinutes; // local HH:MM, in selected zone
    uint16_t sunsetMinutes;
  };
  Events calculateForDate(int year, int month, int day, double lat, double lon);
}

#endif
