#include "WeatherClock.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "Clock.h"
#include "TFTs.h"

namespace
{
constexpr const char *AMBIENT_HOST = "https://api.ambientweather.net/v1/devices";
constexpr uint16_t DEFAULT_REFRESH_MINUTES = 5;
constexpr uint16_t MIN_REFRESH_MINUTES = 1;
constexpr uint16_t MAX_REFRESH_MINUTES = 60;
constexpr uint32_t FETCH_RETRY_AFTER_FAILURE_MS = 60UL * 1000UL;
constexpr uint32_t STALE_AFTER_SECONDS = 30UL * 60UL;
constexpr uint32_t HTTP_TIMEOUT_MS = 12000UL;
}

WeatherClock weather_clock;

WeatherClock::WeatherClock()
    : prefs(), tfts(nullptr), clock(nullptr), mode(normal_mode),
      refresh_minutes(DEFAULT_REFRESH_MINUTES), refresh_requested(true),
      last_attempt_millis(0), last_success_millis(0), reading_valid(false),
      temp_f(0.0f), humidity(0.0f), wind_mph(0.0f), daily_rain_in(0.0f),
      station_date_utc_ms(0), station_count(0), last_render_hour(255),
      last_render_minute(255), reading_generation(0), rendered_generation(0)
{
}

void WeatherClock::begin(TFTs *tfts_, Clock *clock_)
{
  tfts = tfts_;
  clock = clock_;

  prefs.begin("foxweather", false);
  loadConfig();

  if (tfts != nullptr)
    tfts->setWeatherMode(mode == weather_mode);

  refresh_requested = true;

  Serial.print("FoxTube weather: ");
  Serial.println(credentialsConfigured() ? "Ambient Weather credentials configured." : "waiting for Ambient Weather credentials.");
}

void WeatherClock::loadConfig()
{
  api_key = prefs.getString("api", "");
  application_key = prefs.getString("app", "");
  station_mac = prefs.getString("station", "");
  selected_station_name = prefs.getString("stationName", "");

  uint16_t stored_refresh = prefs.getUShort("refresh", DEFAULT_REFRESH_MINUTES);
  refresh_minutes = constrain(stored_refresh, MIN_REFRESH_MINUTES, MAX_REFRESH_MINUTES);

  uint8_t stored_mode = prefs.getUChar("mode", uint8_t(normal_mode));
  mode = stored_mode == uint8_t(weather_mode) ? weather_mode : normal_mode;
}

void WeatherClock::saveConfig()
{
  prefs.putString("station", station_mac);
  prefs.putString("stationName", selected_station_name);
  prefs.putUShort("refresh", refresh_minutes);
  prefs.putUChar("mode", uint8_t(mode));
}

bool WeatherClock::credentialsConfigured() const
{
  return api_key.length() > 0 && application_key.length() > 0;
}

bool WeatherClock::saveCredentials(const String &new_api_key, const String &new_application_key)
{
  String api = new_api_key;
  String app = new_application_key;
  api.trim();
  app.trim();

  if (api.length() == 0 || app.length() == 0)
    return false;

  api_key = api;
  application_key = app;

  prefs.putString("api", api_key);
  prefs.putString("app", application_key);

  // A new account/key pair may expose a different station list.
  station_mac = "";
  selected_station_name = "";
  saveConfig();

  reading_valid = false;
  reading_generation++;
  clearStationList();
  last_error = "";
  requestRefresh();

  Serial.println("FoxTube weather: Ambient Weather credentials saved to NVS.");
  return true;
}

void WeatherClock::clearCredentials()
{
  api_key = "";
  application_key = "";
  station_mac = "";
  selected_station_name = "";
  reading_valid = false;
  reading_generation++;
  station_date_utc_ms = 0;
  last_success_millis = 0;
  last_error = "";
  clearStationList();

  prefs.remove("api");
  prefs.remove("app");
  prefs.remove("station");
  prefs.remove("stationName");

  Serial.println("FoxTube weather: Ambient Weather credentials cleared.");
}

void WeatherClock::setMode(mode_t new_mode)
{
  if (new_mode != normal_mode && new_mode != weather_mode)
    new_mode = normal_mode;

  mode = new_mode;
  prefs.putUChar("mode", uint8_t(mode));

  if (tfts != nullptr)
    tfts->setWeatherMode(mode == weather_mode);

  last_render_hour = 255;
  last_render_minute = 255;
  rendered_generation = UINT32_MAX;

  if (mode == weather_mode)
  {
    requestRefresh();
    render(true);
  }
}

void WeatherClock::toggleMode()
{
  setMode(isWeatherMode() ? normal_mode : weather_mode);
}

void WeatherClock::setRefreshMinutes(uint16_t minutes)
{
  refresh_minutes = constrain(minutes, MIN_REFRESH_MINUTES, MAX_REFRESH_MINUTES);
  prefs.putUShort("refresh", refresh_minutes);
  requestRefresh();
}

void WeatherClock::setStationMac(const String &mac)
{
  String wanted = mac;
  wanted.trim();

  station_mac = wanted;
  selected_station_name = "";

  for (uint8_t i = 0; i < station_count; ++i)
  {
    if (stations[i].mac.equalsIgnoreCase(station_mac))
    {
      selected_station_name = stations[i].name;
      break;
    }
  }

  saveConfig();
  reading_valid = false;
  reading_generation++;
  requestRefresh();
}

void WeatherClock::requestRefresh()
{
  refresh_requested = true;
}

void WeatherClock::clearStationList()
{
  for (uint8_t i = 0; i < MAX_STATIONS; ++i)
  {
    stations[i].mac = "";
    stations[i].name = "";
  }
  station_count = 0;
}

int WeatherClock::findSelectedStation() const
{
  if (station_count == 0)
    return -1;

  if (station_mac.length() == 0)
    return 0;

  for (uint8_t i = 0; i < station_count; ++i)
  {
    if (stations[i].mac.equalsIgnoreCase(station_mac))
      return int(i);
  }

  return -1;
}

bool WeatherClock::parseDevicesJson(const String &payload)
{
  JsonDocument filter;
  filter[0]["macAddress"] = true;
  filter[0]["info"]["name"] = true;
  filter[0]["lastData"]["tempf"] = true;
  filter[0]["lastData"]["humidity"] = true;
  filter[0]["lastData"]["windspeedmph"] = true;
  filter[0]["lastData"]["dailyrainin"] = true;
  filter[0]["lastData"]["dateutc"] = true;

  JsonDocument doc;
  DeserializationError error = deserializeJson(
      doc, payload, DeserializationOption::Filter(filter));

  if (error)
  {
    last_error = String("JSON parse failed: ") + error.c_str();
    return false;
  }

  JsonArray devices = doc.as<JsonArray>();
  if (devices.isNull() || devices.size() == 0)
  {
    last_error = "Ambient Weather returned no stations.";
    return false;
  }

  clearStationList();

  for (JsonObject device : devices)
  {
    const char *mac = device["macAddress"] | "";
    if (mac[0] == '\0')
      continue;

    if (station_count < MAX_STATIONS)
    {
      stations[station_count].mac = mac;

      const char *name = device["info"]["name"] | "";
      if (name[0] != '\0')
        stations[station_count].name = name;
      else
      {
        stations[station_count].name = "Station ";
        stations[station_count].name += String(station_count + 1);
      }

      station_count++;
    }
  }

  int selected = findSelectedStation();

  // First configuration: findSelectedStation() deliberately returns index 0
  // when no station MAC has been saved yet so the UI has a sensible default.
  // We still MUST persist that first station's MAC here; otherwise the later
  // weather-data lookup compares an empty station_mac against every device and
  // reports "Selected station data is unavailable."
  if (station_mac.length() == 0 && station_count > 0)
  {
    selected = 0;
    station_mac = stations[0].mac;
    selected_station_name = stations[0].name;
    saveConfig();

    Serial.print("FoxTube weather: auto-selected station ");
    Serial.print(selected_station_name);
    Serial.print(" (");
    Serial.print(station_mac);
    Serial.println(").");
  }

  if (selected < 0)
  {
    last_error = "Selected Ambient Weather station was not found.";
    return false;
  }

  const String discovered_name = stations[selected].name;
  if (selected_station_name != discovered_name)
  {
    selected_station_name = discovered_name;
    saveConfig();
  }

  // Read the selected station's weather data directly from the matching
  // JsonObject. Do not assign a found JsonObject into a default-constructed
  // JsonObject and inspect it later: with ArduinoJson that can leave the
  // destination object unbound/null even though the station itself matched.
  for (JsonObject device : devices)
  {
    const char *mac = device["macAddress"] | "";
    if (!station_mac.equalsIgnoreCase(String(mac)))
      continue;

    JsonObject last_data = device["lastData"].as<JsonObject>();
    if (last_data.isNull())
    {
      last_error = "Selected station has no current data.";
      return false;
    }

    if (last_data["tempf"].isNull() ||
        last_data["humidity"].isNull() ||
        last_data["windspeedmph"].isNull() ||
        last_data["dailyrainin"].isNull())
    {
      last_error = "Station response is missing required weather fields.";
      return false;
    }

    temp_f = last_data["tempf"].as<float>();
    humidity = last_data["humidity"].as<float>();
    wind_mph = last_data["windspeedmph"].as<float>();
    daily_rain_in = last_data["dailyrainin"].as<float>();
    station_date_utc_ms = last_data["dateutc"] | uint64_t(0);

    reading_valid = true;
    last_error = "";
    last_success_millis = millis();
    reading_generation++;
    return true;
  }

  last_error = "Selected station data is unavailable.";
  return false;
}

bool WeatherClock::fetchNow()
{
  refresh_requested = false;
  last_attempt_millis = millis();

  if (!credentialsConfigured())
  {
    last_error = "Ambient Weather credentials are not configured.";
    return false;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    last_error = "Wi-Fi is not connected.";
    return false;
  }

  WiFiClientSecure tls;
  // The API request is still encrypted, but certificate validation is disabled
  // to avoid pinning a CA/certificate into firmware. FoxTube is not exposed as
  // an HTTPS server; this applies only to its outbound Ambient Weather request.
  tls.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  String url;
  url.reserve(256);
  url = AMBIENT_HOST;
  url += "?apiKey=";
  url += api_key;
  url += "&applicationKey=";
  url += application_key;

  if (!http.begin(tls, url))
  {
    last_error = "Could not initialize Ambient Weather HTTPS request.";
    return false;
  }

  http.setUserAgent(String("FoxTube/") + FIRMWARE_VERSION);
  const int http_code = http.GET();

  if (http_code != HTTP_CODE_OK)
  {
    if (http_code > 0)
      last_error = String("Ambient Weather HTTP ") + http_code;
    else
      last_error = String("Ambient Weather connection failed: ") + http.errorToString(http_code);

    http.end();
    return false;
  }

  // /v1/devices is small for a normal personal account. Keeping the body in a
  // String also lets HTTPClient transparently handle chunked transfer encoding.
  String payload = http.getString();
  http.end();

  const bool ok = parseDevicesJson(payload);

  if (ok)
  {
    Serial.print("FoxTube weather: updated ");
    Serial.print(selected_station_name);
    Serial.print(" (");
    Serial.print(temp_f, 1);
    Serial.println(" F).");

  }
  else
  {
    Serial.print("FoxTube weather: ");
    Serial.println(last_error);
  }

  return ok;
}

uint32_t WeatherClock::getLastSuccessAgeSeconds() const
{
  if (!reading_valid)
    return 0;

  // Prefer the station's own timestamp so stale data is detected even when
  // the Ambient API request itself succeeds but the weather station has
  // stopped uploading new observations.
  if (station_date_utc_ms > 0 && clock != nullptr && clock->loop_time > 0)
  {
    const uint64_t now_ms = uint64_t(clock->loop_time) * 1000ULL;
    if (now_ms >= station_date_utc_ms)
      return uint32_t((now_ms - station_date_utc_ms) / 1000ULL);
  }

  if (last_success_millis == 0)
    return 0;

  return (millis() - last_success_millis) / 1000UL;
}

bool WeatherClock::isStale() const
{
  return reading_valid && getLastSuccessAgeSeconds() > STALE_AFTER_SECONDS;
}

void WeatherClock::loop()
{
  if (!credentialsConfigured() || WiFi.status() != WL_CONNECTED)
    return;

  const uint32_t now = millis();
  const uint32_t refresh_ms = uint32_t(refresh_minutes) * 60UL * 1000UL;

  bool due = refresh_requested;
  if (!due && last_success_millis != 0)
    due = (now - last_success_millis) >= refresh_ms;
  else if (!due && last_attempt_millis != 0 && last_success_millis == 0)
    due = (now - last_attempt_millis) >= FETCH_RETRY_AFTER_FAILURE_MS;
  else if (!due && last_attempt_millis == 0)
    due = true;

  if (due)
    fetchNow();
}

void WeatherClock::render(bool force)
{
  if (!isWeatherMode() || tfts == nullptr || clock == nullptr)
    return;

  if (!tfts->isEnabled() || tfts->isPanoramaMode())
    return;

  const uint8_t hour = clock->getHour();
  const uint8_t minute = clock->getMinute();

  if (!force &&
      last_render_hour == hour &&
      last_render_minute == minute &&
      rendered_generation == reading_generation)
  {
    return;
  }

  tfts->drawWeatherClock(
      clock->getHoursTens(),
      clock->getHoursOnes(),
      clock->getMinutesTens(),
      clock->getMinutesOnes(),
      reading_valid,
      temp_f,
      humidity,
      wind_mph,
      daily_rain_in,
      isStale(),
      getLastSuccessAgeSeconds());

  last_render_hour = hour;
  last_render_minute = minute;
  rendered_generation = reading_generation;
}

#endif // HARDWARE_IPSTUBE_S3_CLOCK
