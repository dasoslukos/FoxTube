#include "WebUI.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <math.h>

#include "Backlights.h"
#include "Clock.h"
#include "DisplaySchedule.h"
#include "StoredConfig.h"
#include "TFTs.h"
#include "WeatherClock.h"

static const char FOXTube_WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FoxTube</title>
<style>
:root{color-scheme:dark;--bg:#09070f;--card:#151020;--text:#f7eefc;--muted:#aa9bb8;--orange:#ff8a36;--purple:#9f68ff;--blue:#5aa7ff;--green:#44d79d;--red:#ff5f78;--line:#30253f}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 20% 0,#25133a 0,transparent 34%),radial-gradient(circle at 80% 10%,#132649 0,transparent 28%),var(--bg);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:var(--text);min-height:100vh}
.wrap{width:min(980px,calc(100% - 28px));margin:0 auto;padding:28px 0 54px}.hero{display:flex;gap:16px;align-items:center;margin-bottom:22px}.fox{font-size:54px;filter:drop-shadow(0 0 14px #ff7a32)}h1{margin:0;font-size:34px}.sub{color:var(--muted);margin-top:5px}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.card{background:linear-gradient(145deg,rgba(27,20,40,.96),rgba(15,12,24,.96));border:1px solid var(--line);border-radius:18px;padding:18px;box-shadow:0 12px 34px #0007}.wide{grid-column:1/-1}.card h2{font-size:18px;margin:0 0 15px}.accent-orange{border-top:2px solid var(--orange)}.accent-purple{border-top:2px solid var(--purple)}.accent-blue{border-top:2px solid var(--blue)}.accent-green{border-top:2px solid var(--green)}.row{display:grid;grid-template-columns:150px 1fr auto;gap:12px;align-items:center;margin:12px 0}.label{font-size:14px;color:#d9cce4}.value{font-variant-numeric:tabular-nums;color:var(--muted);font-size:13px;min-width:38px;text-align:right}select,input[type=range],input[type=color],input[type=time],input[type=password],button{width:100%}select,input[type=time],input[type=password],button{background:#100c18;color:var(--text);border:1px solid #403151;border-radius:10px;padding:10px 12px;font-size:14px}input[type=color]{height:40px;border:1px solid #403151;border-radius:10px;padding:3px;background:#100c18}input[type=range]{accent-color:var(--purple)}.toggle{display:flex;gap:8px}.toggle button.active{border-color:var(--orange);box-shadow:0 0 0 1px var(--orange) inset;color:#fff}.modebuttons{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}.modebuttons button.active{border-color:var(--orange);box-shadow:0 0 0 1px var(--orange) inset;background:#221126}.big{padding:13px 16px;font-weight:700;background:linear-gradient(90deg,#7b45db,#d764bc,#ef7d35);border:0;cursor:pointer}.danger{border-color:#783040;color:#ffb6c2}.status{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.pill{background:#0d0a13;border:1px solid #2d2338;border-radius:12px;padding:11px}.pill b{display:block;font-size:12px;color:var(--muted);margin-bottom:4px}.pill span{font-size:14px}.wxstatus{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:14px}.toast{position:fixed;right:18px;bottom:18px;background:#171021;border:1px solid #714da0;border-radius:12px;padding:10px 14px;opacity:0;transform:translateY(8px);transition:.2s;pointer-events:none;max-width:min(420px,calc(100% - 36px))}.toast.show{opacity:1;transform:none}.toast.bad{border-color:var(--red);color:#ffd6dc}.hint{font-size:12px;color:var(--muted);margin-top:10px;line-height:1.5}.mode{display:inline-block;padding:5px 9px;border:1px solid #403151;border-radius:999px;color:var(--muted);font-size:12px}.mode.night{border-color:var(--purple);color:#d9c6ff}.mode.day{border-color:var(--orange);color:#ffd1ad}.good{color:var(--green)}.badtext{color:#ff8798}.credentials{display:grid;grid-template-columns:1fr 1fr;gap:12px}.actions{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin-top:12px}
@media(max-width:720px){.grid{grid-template-columns:1fr}.wide{grid-column:auto}.row{grid-template-columns:110px 1fr auto}.status,.wxstatus{grid-template-columns:repeat(2,1fr)}.credentials{grid-template-columns:1fr}.actions{grid-template-columns:1fr}.modebuttons{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="wrap">
  <div class="hero"><div class="fox">🦊</div><div><h1>FoxTube</h1><div class="sub">Local control den · no cloud required</div></div></div>

  <div class="grid">
    <section class="card accent-orange">
      <h2>Tube LEDs · pixels 0–5</h2>
      <div class="row"><div class="label">Pattern</div><select id="tubePattern"></select><span></span></div>
      <div class="row"><div class="label">Color</div><input id="tubeColor" type="color"><span></span></div>
      <div class="row"><div class="label">Brightness</div><input id="tubeIntensity" type="range" min="0" max="7" step="1"><div class="value" id="tubeIntensityValue"></div></div>
    </section>

    <section class="card accent-purple" id="stripCard">
      <h2>Bottom Strip · pixels 6–33</h2>
      <div class="row"><div class="label">Pattern</div><select id="stripPattern"></select><span></span></div>
      <div class="row"><div class="label">Color</div><input id="stripColor" type="color"><span></span></div>
      <div class="row"><div class="label">Brightness</div><input id="stripIntensity" type="range" min="0" max="7" step="1"><div class="value" id="stripIntensityValue"></div></div>
    </section>

    <section class="card accent-blue">
      <h2>Clock</h2>
      <div class="row"><div class="label">Clock face</div><select id="face"></select><span></span></div>
      <div class="row"><div class="label">Hour format</div><div class="toggle"><button id="h12">12 hour</button><button id="h24">24 hour</button></div><span></span></div>
      <div class="row"><div class="label">Leading zero</div><div class="toggle"><button id="zeroOn">Show</button><button id="zeroOff">Blank</button></div><span></span></div>
    </section>

    <section class="card accent-orange">
      <h2>Clock Mode</h2>
      <div class="modebuttons">
        <button id="modeNormal">Normal</button>
        <button id="modeWeather">Weather</button>
        <button id="modeCycle">Cycle</button>
        <button id="modePanorama">Panorama</button>
      </div>
      <div class="row">
        <div class="label">Cycle interval</div>
        <input id="cycleSeconds" type="range" min="5" max="300" step="5">
        <div class="value" id="cycleSecondsValue">30 sec</div>
      </div>
      <div class="hint" id="cycleView">Cycle alternates Normal and Weather without changing the Ambient Weather refresh interval.</div>
      <div class="hint">Weather mode displays <b>HH : MM</b> with live Ambient Weather on the right-most tube. Panorama still uses 100.bmp–105.bmp.</div>
    </section>

    <section class="card wide accent-green">
      <h2>Weather Station</h2>
      <div id="weatherConfigured" class="hint">Ambient Weather credentials are not configured.</div>

      <div class="credentials">
        <div>
          <div class="label">API Key</div>
          <input id="weatherApiKey" type="password" autocomplete="off" placeholder="Enter Ambient API key">
        </div>
        <div>
          <div class="label">Application Key</div>
          <input id="weatherAppKey" type="password" autocomplete="off" placeholder="Enter Ambient application key">
        </div>
      </div>

      <div class="actions">
        <button class="big" id="weatherSave">Save Credentials</button>
        <button id="weatherTest">Test Connection</button>
        <button class="danger" id="weatherClear">Clear Credentials</button>
      </div>

      <div class="row"><div class="label">Station</div><select id="weatherStation"></select><span></span></div>
      <div class="row"><div class="label">Refresh</div><select id="weatherRefresh">
        <option value="1">1 minute</option>
        <option value="5">5 minutes</option>
        <option value="10">10 minutes</option>
        <option value="15">15 minutes</option>
        <option value="30">30 minutes</option>
        <option value="60">60 minutes</option>
      </select><span></span></div>

      <div class="wxstatus">
        <div class="pill"><b>Temperature</b><span id="wxTemp">—</span></div>
        <div class="pill"><b>Humidity</b><span id="wxHumidity">—</span></div>
        <div class="pill"><b>Wind</b><span id="wxWind">—</span></div>
        <div class="pill"><b>Rain today</b><span id="wxRain">—</span></div>
      </div>
      <div class="hint" id="weatherAge">No weather reading yet.</div>
      <div class="hint">Credentials are stored only in ESP32 NVS and are never returned by this page after saving.</div>
    </section>

    <section class="card wide accent-blue">
      <h2>Display Day / Night</h2>
      <div class="row"><div class="label">Auto schedule</div><div class="toggle"><button id="displayAutoOn">On</button><button id="displayAutoOff">Off</button></div><span class="mode" id="displayMode">—</span></div>
      <div class="row"><div class="label">Day starts</div><input id="dayStart" type="time" step="60"><span></span></div>
      <div class="row"><div class="label">Day brightness</div><input id="dayBrightness" type="range" min="0" max="255" step="1"><div class="value" id="dayBrightnessValue"></div></div>
      <div class="row"><div class="label">Night starts</div><input id="nightStart" type="time" step="60"><span></span></div>
      <div class="row"><div class="label">Night brightness</div><input id="nightBrightness" type="range" min="0" max="255" step="1"><div class="value" id="nightBrightnessValue"></div></div>
      <div class="hint">Controls the six TFT screens only. Tube LEDs and the bottom strip keep their own independent brightness settings.</div>
    </section>

    <section class="card wide">
      <h2>FoxTube status</h2>
      <div class="status">
        <div class="pill"><b>Address</b><span id="ip">—</span></div>
        <div class="pill"><b>mDNS</b><span>foxtube.local</span></div>
        <div class="pill"><b>Wi‑Fi RSSI</b><span id="rssi">—</span></div>
        <div class="pill"><b>Firmware</b><span id="version">—</span></div>
      </div>
    </section>
  </div>
</div>
<div class="toast" id="toast">Saved ✨</div>
<script>
const $=id=>document.getElementById(id),patterns=['Dark','Test','Constant','Rainbow','Pulse','Breath'];let state={};
function fillPatterns(el){el.innerHTML=patterns.map((p,i)=>`<option value="${i}">${p}</option>`).join('')}
fillPatterns($('tubePattern'));fillPatterns($('stripPattern'));
function showToast(msg='Saved ✨',bad=false){const t=$('toast');t.textContent=msg;t.classList.toggle('bad',bad);t.classList.add('show');setTimeout(()=>t.classList.remove('show'),1400)}
async function post(url,data={},quiet=false){const body=new URLSearchParams(data);const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});if(!r.ok){const msg=await r.text();showToast(msg||'Request failed',true);throw new Error(msg)}if(!quiet)showToast();await loadState();return r}
function active(id,on){$(id).classList.toggle('active',!!on)}
function num(v,d=1){return Number(v).toFixed(d)}
function apply(s){
state=s;
$('tubePattern').value=s.tube.pattern;$('tubeColor').value=s.tube.color;$('tubeIntensity').value=s.tube.intensity;$('tubeIntensityValue').textContent=s.tube.intensity+'/7';
if(s.strip){$('stripPattern').value=s.strip.pattern;$('stripColor').value=s.strip.color;$('stripIntensity').value=s.strip.intensity;$('stripIntensityValue').textContent=s.strip.intensity+'/7'}else{$('stripCard').style.display='none'}
$('face').innerHTML=Array.from({length:s.clock.faces},(_,i)=>`<option value="${i+1}">Face ${i+1}${i+1===8?' · Fox Den':''}</option>`).join('');$('face').value=s.clock.face;
active('h12',s.clock.twelveHour);active('h24',!s.clock.twelveHour);active('zeroOn',!s.clock.blankZero);active('zeroOff',s.clock.blankZero);
if(s.display){active('displayAutoOn',s.display.enabled);active('displayAutoOff',!s.display.enabled);$('dayStart').value=s.display.dayStart;$('nightStart').value=s.display.nightStart;$('dayBrightness').value=s.display.dayBrightness;$('nightBrightness').value=s.display.nightBrightness;$('dayBrightnessValue').textContent=s.display.dayBrightness+'/255';$('nightBrightnessValue').textContent=s.display.nightBrightness+'/255';const dm=$('displayMode');dm.textContent=(s.display.isNight?'Night':'Day')+' · '+s.display.appliedBrightness;dm.classList.toggle('night',s.display.isNight);dm.classList.toggle('day',!s.display.isNight)}
const mode=s.panorama?'panorama':((s.weather&&s.weather.mode)||'normal');active('modeNormal',mode==='normal');active('modeWeather',mode==='weather');active('modeCycle',mode==='cycle');active('modePanorama',mode==='panorama');
if(s.weather){
  const c=$('weatherConfigured');c.textContent=s.weather.configured?'✓ Ambient Weather configured':'Ambient Weather credentials are not configured.';c.classList.toggle('good',s.weather.configured);c.classList.toggle('badtext',!s.weather.configured);
  $('cycleSeconds').value=String(s.weather.cycleSeconds);$('cycleSecondsValue').textContent=s.weather.cycleSeconds+' sec';
  const cv=$('cycleView');if(s.weather.mode==='cycle'){cv.textContent=(s.panorama?'Cycle is underneath Panorama · ':'Cycle active · ')+(s.weather.showingWeather?'currently Weather':'currently Normal')}else{cv.textContent='Cycle alternates Normal and Weather without changing the Ambient Weather refresh interval.'}
  $('weatherRefresh').value=String(s.weather.refreshMinutes);
  const st=$('weatherStation');
  if(s.weather.stations&&s.weather.stations.length){st.innerHTML=s.weather.stations.map(x=>`<option value="${x.mac}">${x.name}</option>`).join('');st.value=s.weather.stationMac||s.weather.stations[0].mac;st.disabled=false}else{st.innerHTML='<option value="">Test connection to load stations</option>';st.disabled=true}
  if(s.weather.hasData){$('wxTemp').textContent=num(s.weather.tempF,1)+'°F';$('wxHumidity').textContent=num(s.weather.humidity,0)+'%';$('wxWind').textContent=num(s.weather.windMph,1)+' mph';$('wxRain').textContent=num(s.weather.dailyRainIn,2)+' in';$('weatherAge').textContent=(s.weather.stale?'STALE · ':'')+(s.weather.ageSeconds<60?'updated just now':'updated '+Math.floor(s.weather.ageSeconds/60)+' min ago')+(s.weather.stationName?' · '+s.weather.stationName:'')}else{$('wxTemp').textContent=$('wxHumidity').textContent=$('wxWind').textContent=$('wxRain').textContent='—';$('weatherAge').textContent=s.weather.lastError||'No weather reading yet.'}
}
$('ip').textContent=s.device.ip;$('rssi').textContent=s.device.rssi+' dBm';$('version').textContent=s.device.version
}
async function loadState(){try{const r=await fetch('/api/state',{cache:'no-store'});if(r.ok)apply(await r.json())}catch(e){console.log(e)}}
$('tubePattern').onchange=e=>post('/api/tube',{pattern:e.target.value});$('tubeColor').onchange=e=>post('/api/tube',{color:e.target.value});$('tubeIntensity').oninput=e=>$('tubeIntensityValue').textContent=e.target.value+'/7';$('tubeIntensity').onchange=e=>post('/api/tube',{intensity:e.target.value});
$('stripPattern').onchange=e=>post('/api/strip',{pattern:e.target.value});$('stripColor').onchange=e=>post('/api/strip',{color:e.target.value});$('stripIntensity').oninput=e=>$('stripIntensityValue').textContent=e.target.value+'/7';$('stripIntensity').onchange=e=>post('/api/strip',{intensity:e.target.value});
$('face').onchange=e=>post('/api/clock',{face:e.target.value});$('h12').onclick=()=>post('/api/clock',{twelve:'1'});$('h24').onclick=()=>post('/api/clock',{twelve:'0'});$('zeroOn').onclick=()=>post('/api/clock',{blank:'0'});$('zeroOff').onclick=()=>post('/api/clock',{blank:'1'});
$('modeNormal').onclick=()=>post('/api/mode',{mode:'normal'});$('modeWeather').onclick=()=>post('/api/mode',{mode:'weather'});$('modeCycle').onclick=()=>post('/api/mode',{mode:'cycle'});$('modePanorama').onclick=()=>post('/api/mode',{mode:'panorama'});$('cycleSeconds').oninput=e=>$('cycleSecondsValue').textContent=e.target.value+' sec';$('cycleSeconds').onchange=e=>post('/api/mode',{cycleSeconds:e.target.value});
$('weatherSave').onclick=async()=>{const api=$('weatherApiKey').value.trim(),app=$('weatherAppKey').value.trim();if(!api||!app){showToast('Enter both Ambient Weather keys',true);return}await post('/api/weather/credentials',{apiKey:api,applicationKey:app});$('weatherApiKey').value='';$('weatherAppKey').value=''};
$('weatherTest').onclick=async()=>{try{await post('/api/weather/test',{},true);showToast('Weather connection works 🦊')}catch(e){}};
$('weatherClear').onclick=()=>{if(confirm('Clear Ambient Weather credentials from FoxTube?'))post('/api/weather/clear')};
$('weatherStation').onchange=e=>post('/api/weather/config',{station:e.target.value});$('weatherRefresh').onchange=e=>post('/api/weather/config',{refresh:e.target.value});
$('displayAutoOn').onclick=()=>post('/api/display',{enabled:'1'});$('displayAutoOff').onclick=()=>post('/api/display',{enabled:'0'});$('dayStart').onchange=e=>post('/api/display',{dayStart:e.target.value});$('nightStart').onchange=e=>post('/api/display',{nightStart:e.target.value});$('dayBrightness').oninput=e=>$('dayBrightnessValue').textContent=e.target.value+'/255';$('dayBrightness').onchange=e=>post('/api/display',{dayBrightness:e.target.value});$('nightBrightness').oninput=e=>$('nightBrightnessValue').textContent=e.target.value+'/255';$('nightBrightness').onchange=e=>post('/api/display',{nightBrightness:e.target.value});
loadState();setInterval(loadState,15000);
</script>
</body>
</html>
)HTML";

WebUI::WebUI()
    : server(80), backlights(nullptr), tfts(nullptr), clock(nullptr),
      stored_config(nullptr), display_schedule(nullptr), weather_clock(nullptr),
      started(false), mdns_started(false)
{
}

void WebUI::begin(Backlights *backlights_, TFTs *tfts_, Clock *clock_,
                  StoredConfig *stored_config_, DisplaySchedule *display_schedule_,
                  WeatherClock *weather_clock_)
{
  backlights = backlights_;
  tfts = tfts_;
  clock = clock_;
  stored_config = stored_config_;
  display_schedule = display_schedule_;
  weather_clock = weather_clock_;

  installRoutes();
  server.begin();
  started = true;

  Serial.print("FoxTube Web UI: http://");
  Serial.println(WiFi.localIP());

  if (WiFi.status() == WL_CONNECTED)
  {
    if (MDNS.begin("foxtube"))
    {
      MDNS.addService("http", "tcp", 80);
      mdns_started = true;
      Serial.println("FoxTube Web UI mDNS: http://foxtube.local/");
    }
    else
    {
      Serial.println("FoxTube Web UI: mDNS start failed; use the IP address.");
    }
  }
}

void WebUI::loop()
{
  if (!started)
    return;

  server.handleClient();

  if (!mdns_started && WiFi.status() == WL_CONNECTED)
  {
    if (MDNS.begin("foxtube"))
    {
      MDNS.addService("http", "tcp", 80);
      mdns_started = true;
      Serial.println("FoxTube Web UI mDNS now available: http://foxtube.local/");
    }
  }
}

void WebUI::installRoutes()
{
  server.on("/", HTTP_GET, [this]() { handleRoot(); });
  server.on("/api/state", HTTP_GET, [this]() { handleState(); });
  server.on("/api/tube", HTTP_POST, [this]() { handleTube(); });
  server.on("/api/strip", HTTP_POST, [this]() { handleStrip(); });
  server.on("/api/clock", HTTP_POST, [this]() { handleClock(); });
  server.on("/api/panorama", HTTP_POST, [this]() { handlePanorama(); });
  server.on("/api/display", HTTP_POST, [this]() { handleDisplay(); });
  server.on("/api/mode", HTTP_POST, [this]() { handleMode(); });
  server.on("/api/weather/credentials", HTTP_POST, [this]() { handleWeatherCredentials(); });
  server.on("/api/weather/config", HTTP_POST, [this]() { handleWeatherConfig(); });
  server.on("/api/weather/test", HTTP_POST, [this]() { handleWeatherTest(); });
  server.on("/api/weather/clear", HTTP_POST, [this]() { handleWeatherClear(); });
  server.onNotFound([this]() { handleNotFound(); });
}

void WebUI::handleRoot()
{
  server.send_P(200, "text/html; charset=utf-8", FOXTube_WEB_PAGE);
}

String WebUI::colorToHtml(uint32_t color)
{
  char buffer[8];
  snprintf(buffer, sizeof(buffer), "#%06lX", (unsigned long)(color & 0xFFFFFF));
  return String(buffer);
}

String WebUI::jsonEscape(const String &value)
{
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i)
  {
    const char c = value[i];
    if (c == '\\' || c == '"')
    {
      out += '\\';
      out += c;
    }
    else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else
      out += c;
  }
  return out;
}

bool WebUI::parseTimeMinutes(const String &value, uint16_t &minutes)
{
  if (value.length() != 5 || value.charAt(2) != ':')
    return false;

  const int hour = value.substring(0, 2).toInt();
  const int minute = value.substring(3, 5).toInt();
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
    return false;

  minutes = uint16_t(hour * 60 + minute);
  return true;
}

void WebUI::handleState()
{
  String json;
  json.reserve(1900);

  json += "{\"tube\":{";
  json += "\"pattern\":";
  json += String((int)backlights->getPattern());
  json += ",\"color\":\"";
  json += colorToHtml(backlights->getColor());
  json += "\",\"intensity\":";
  json += String(backlights->getIntensity());
  json += "},";

#ifdef HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
  json += "\"strip\":{";
  json += "\"pattern\":";
  json += String((int)backlights->getStripPattern());
  json += ",\"color\":\"";
  json += colorToHtml(backlights->getStripColor());
  json += "\",\"intensity\":";
  json += String(backlights->getStripIntensity());
  json += "},";
#else
  json += "\"strip\":null,";
#endif

  json += "\"clock\":{";
  json += "\"face\":";
  json += String(clock->getActiveGraphicIdx());
  json += ",\"faces\":";
  json += String(tfts->NumberOfClockFaces);
  json += ",\"twelveHour\":";
  json += clock->getTwelveHour() ? "true" : "false";
  json += ",\"blankZero\":";
  json += clock->getBlankHoursZero() ? "true" : "false";
  json += "},";

  json += "\"display\":{";
  json += "\"enabled\":";
  json += display_schedule->getEnabled() ? "true" : "false";

  const uint16_t day_minutes = display_schedule->getDayStartMinutes();
  const uint16_t night_minutes = display_schedule->getNightStartMinutes();
  char time_buffer[6];

  snprintf(time_buffer, sizeof(time_buffer), "%02u:%02u", day_minutes / 60, day_minutes % 60);
  json += ",\"dayStart\":\"";
  json += time_buffer;
  json += "\"";

  snprintf(time_buffer, sizeof(time_buffer), "%02u:%02u", night_minutes / 60, night_minutes % 60);
  json += ",\"nightStart\":\"";
  json += time_buffer;
  json += "\"";

  json += ",\"dayBrightness\":";
  json += String(display_schedule->getDayBrightness());
  json += ",\"nightBrightness\":";
  json += String(display_schedule->getNightBrightness());
  json += ",\"isNight\":";
  json += display_schedule->isNight(clock->getHour24(), clock->getMinute()) ? "true" : "false";
  json += ",\"appliedBrightness\":";
  json += String(display_schedule->getAppliedBrightness());
  json += "},";

  json += "\"weather\":{";
  json += "\"configured\":";
  json += weather_clock->credentialsConfigured() ? "true" : "false";
  json += ",\"mode\":\"";
  json += weather_clock->getModeName();
  json += "\",\"showingWeather\":";
  json += weather_clock->isShowingWeather() ? "true" : "false";
  json += ",\"cycleSeconds\":";
  json += String(weather_clock->getCycleSeconds());
  json += ",\"refreshMinutes\":";
  json += String(weather_clock->getRefreshMinutes());
  json += ",\"stationMac\":\"";
  json += jsonEscape(weather_clock->getStationMac());
  json += "\",\"stationName\":\"";
  json += jsonEscape(weather_clock->getStationName());
  json += "\",\"hasData\":";
  json += weather_clock->hasReading() ? "true" : "false";
  json += ",\"tempF\":";
  json += String(weather_clock->getTemperatureF(), 2);
  json += ",\"humidity\":";
  json += String(weather_clock->getHumidity(), 1);
  json += ",\"windMph\":";
  json += String(weather_clock->getWindMph(), 2);
  json += ",\"dailyRainIn\":";
  json += String(weather_clock->getDailyRainIn(), 3);
  json += ",\"ageSeconds\":";
  json += String(weather_clock->getLastSuccessAgeSeconds());
  json += ",\"stale\":";
  json += weather_clock->isStale() ? "true" : "false";
  json += ",\"lastError\":\"";
  json += jsonEscape(weather_clock->getLastError());
  json += "\",\"stations\":[";

  for (uint8_t i = 0; i < weather_clock->getStationCount(); ++i)
  {
    if (i)
      json += ",";
    const WeatherClock::StationInfo &station = weather_clock->getStation(i);
    json += "{\"mac\":\"";
    json += jsonEscape(station.mac);
    json += "\",\"name\":\"";
    json += jsonEscape(station.name);
    json += "\"}";
  }
  json += "]},";

  json += "\"panorama\":";
  json += tfts->isPanoramaMode() ? "true" : "false";
  json += ",\"device\":{";
  json += "\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",\"rssi\":";
  json += String(WiFi.RSSI());
  json += ",\"version\":\"";
  json += jsonEscape(String(FIRMWARE_VERSION));
  json += "\",\"uptime\":";
  json += String(millis() / 1000UL);
  json += "}}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

uint16_t WebUI::htmlColorToPhase(const String &value)
{
  String hex = value;
  hex.trim();
  if (hex.startsWith("#"))
    hex.remove(0, 1);

  if (hex.length() != 6)
    return 0;

  char *endptr = nullptr;
  const uint32_t rgb = strtoul(hex.c_str(), &endptr, 16);
  if (endptr == hex.c_str())
    return 0;

  const float r = float((rgb >> 16) & 0xFF) / 255.0f;
  const float g = float((rgb >> 8) & 0xFF) / 255.0f;
  const float b = float(rgb & 0xFF) / 255.0f;
  const float maxc = max(r, max(g, b));
  const float minc = min(r, min(g, b));
  const float delta = maxc - minc;

  float hue = 0.0f;
  if (delta > 0.0001f)
  {
    if (maxc == r)
      hue = 60.0f * fmodf(((g - b) / delta), 6.0f);
    else if (maxc == g)
      hue = 60.0f * (((b - r) / delta) + 2.0f);
    else
      hue = 60.0f * (((r - g) / delta) + 4.0f);
  }
  if (hue < 0.0f)
    hue += 360.0f;

  return uint16_t(backlights->hueToPhase(hue));
}

void WebUI::handleTube()
{
  if (server.hasArg("pattern"))
  {
    const int p = server.arg("pattern").toInt();
    if (p >= 0 && p < Backlights::num_patterns)
      backlights->setPattern(Backlights::patterns(p));
  }

  if (server.hasArg("color"))
    backlights->setColorPhase(htmlColorToPhase(server.arg("color")));

  if (server.hasArg("intensity"))
  {
    int intensity = server.arg("intensity").toInt();
    intensity = constrain(intensity, 0, int(backlights->max_intensity - 1));
    backlights->setIntensity(uint8_t(intensity));
  }

  stored_config->save();
  sendOk();
}

void WebUI::handleStrip()
{
#ifdef HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
  if (server.hasArg("pattern"))
  {
    const int p = server.arg("pattern").toInt();
    if (p >= 0 && p < Backlights::num_patterns)
      backlights->setStripPattern(Backlights::patterns(p));
  }

  if (server.hasArg("color"))
    backlights->setStripColorPhase(htmlColorToPhase(server.arg("color")));

  if (server.hasArg("intensity"))
  {
    int intensity = server.arg("intensity").toInt();
    intensity = constrain(intensity, 0, int(backlights->max_intensity - 1));
    backlights->setStripIntensity(uint8_t(intensity));
  }

  backlights->saveStripConfig();
  sendOk();
#else
  server.send(404, "text/plain", "Bottom strip support is not enabled in this build.");
#endif
}

void WebUI::redrawClock()
{
  if (!tfts->isEnabled() || tfts->isPanoramaMode())
    return;

  if (weather_clock != nullptr && weather_clock->isShowingWeather())
  {
    weather_clock->render(true);
    return;
  }

  tfts->setDigit(SECONDS_ONES, clock->getSecondsOnes(), TFTs::force);
  tfts->setDigit(SECONDS_TENS, clock->getSecondsTens(), TFTs::force);
  tfts->setDigit(MINUTES_ONES, clock->getMinutesOnes(), TFTs::force);
  tfts->setDigit(MINUTES_TENS, clock->getMinutesTens(), TFTs::force);
  tfts->setDigit(HOURS_ONES, clock->getHoursOnes(), TFTs::force);
  tfts->setDigit(HOURS_TENS, clock->getHoursTens(), TFTs::force);
}

void WebUI::handleClock()
{
  if (server.hasArg("face"))
  {
    int face = server.arg("face").toInt();
    face = constrain(face, 1, int(tfts->NumberOfClockFaces));
    clock->setClockGraphicsIdx(int8_t(face));
    tfts->current_graphic = clock->getActiveGraphicIdx();
  }

  if (server.hasArg("twelve"))
    clock->setTwelveHour(server.arg("twelve").toInt() != 0);

  if (server.hasArg("blank"))
    clock->setBlankHoursZero(server.arg("blank").toInt() != 0);

  stored_config->save();
  redrawClock();
  sendOk();
}

void WebUI::handleDisplay()
{
  if (display_schedule == nullptr)
  {
    server.send(503, "text/plain", "Display schedule is not available.");
    return;
  }

  if (server.hasArg("enabled"))
    display_schedule->setEnabled(server.arg("enabled").toInt() != 0);

  uint16_t minutes = 0;
  if (server.hasArg("dayStart"))
  {
    if (!parseTimeMinutes(server.arg("dayStart"), minutes))
    {
      server.send(400, "text/plain", "Invalid dayStart time.");
      return;
    }
    display_schedule->setDayStartMinutes(minutes);
  }

  if (server.hasArg("nightStart"))
  {
    if (!parseTimeMinutes(server.arg("nightStart"), minutes))
    {
      server.send(400, "text/plain", "Invalid nightStart time.");
      return;
    }
    display_schedule->setNightStartMinutes(minutes);
  }

  if (server.hasArg("dayBrightness"))
  {
    const int brightness = constrain(server.arg("dayBrightness").toInt(), 0, 255);
    display_schedule->setDayBrightness(uint8_t(brightness));
  }

  if (server.hasArg("nightBrightness"))
  {
    const int brightness = constrain(server.arg("nightBrightness").toInt(), 0, 255);
    display_schedule->setNightBrightness(uint8_t(brightness));
  }

  display_schedule->applyNow(clock->getHour24(), clock->getMinute());
  sendOk();
}

void WebUI::handleMode()
{
  bool changed = false;

  if (server.hasArg("cycleSeconds"))
  {
    const int seconds = constrain(server.arg("cycleSeconds").toInt(), 5, 300);
    weather_clock->setCycleSeconds(uint16_t(seconds));
    changed = true;
  }

  if (server.hasArg("mode"))
  {
    const String requested = server.arg("mode");

    if (requested == "panorama")
    {
      if (!tfts->isPanoramaMode())
        tfts->enablePanorama(100);
    }
    else if (requested == "weather")
    {
      if (tfts->isPanoramaMode())
        tfts->disablePanorama();

      weather_clock->setMode(WeatherClock::weather_mode);
      weather_clock->render(true);
    }
    else if (requested == "cycle")
    {
      if (tfts->isPanoramaMode())
        tfts->disablePanorama();

      weather_clock->setMode(WeatherClock::cycle_mode);
      redrawClock(); // Cycle always begins with a full Normal view.
    }
    else if (requested == "normal")
    {
      if (tfts->isPanoramaMode())
        tfts->disablePanorama();

      weather_clock->setMode(WeatherClock::normal_mode);
      redrawClock();
    }
    else
    {
      server.send(400, "text/plain", "Unknown display mode.");
      return;
    }

    changed = true;
  }

  if (!changed)
  {
    server.send(400, "text/plain", "Missing display mode or cycle interval.");
    return;
  }

  sendOk();
}

void WebUI::handleWeatherCredentials()
{
  if (!server.hasArg("apiKey") || !server.hasArg("applicationKey"))
  {
    server.send(400, "text/plain", "Both Ambient Weather keys are required.");
    return;
  }

  if (!weather_clock->saveCredentials(server.arg("apiKey"), server.arg("applicationKey")))
  {
    server.send(400, "text/plain", "Ambient Weather keys cannot be blank.");
    return;
  }

  sendOk();
}

void WebUI::handleWeatherConfig()
{
  if (server.hasArg("refresh"))
  {
    const int minutes = constrain(server.arg("refresh").toInt(), 1, 60);
    weather_clock->setRefreshMinutes(uint16_t(minutes));
  }

  if (server.hasArg("station"))
    weather_clock->setStationMac(server.arg("station"));

  sendOk();
}

void WebUI::handleWeatherTest()
{
  if (!weather_clock->credentialsConfigured())
  {
    server.send(400, "text/plain", "Save Ambient Weather credentials first.");
    return;
  }

  if (!weather_clock->fetchNow())
  {
    server.send(502, "text/plain", weather_clock->getLastError());
    return;
  }

  sendOk();
}

void WebUI::handleWeatherClear()
{
  weather_clock->clearCredentials();
  sendOk();
}

void WebUI::handlePanorama()
{
  bool enable = !tfts->isPanoramaMode();
  if (server.hasArg("enabled"))
    enable = server.arg("enabled").toInt() != 0;

  if (enable && !tfts->isPanoramaMode())
  {
    tfts->enablePanorama(100);
  }
  else if (!enable && tfts->isPanoramaMode())
  {
    tfts->disablePanorama();
    redrawClock();
  }

  sendOk();
}

void WebUI::sendOk()
{
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"ok\":true}");
}

void WebUI::handleNotFound()
{
  server.send(404, "text/plain", "Fox wandered off. 404.");
}

#endif // HARDWARE_IPSTUBE_S3_CLOCK
