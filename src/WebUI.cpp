#include "WebUI.h"

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <math.h>

#include "Backlights.h"
#include "Clock.h"
#include "StoredConfig.h"
#include "TFTs.h"

static const char FOXTube_WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FoxTube</title>
<style>
:root{color-scheme:dark;--bg:#09070f;--card:#151020;--card2:#1b1428;--text:#f7eefc;--muted:#aa9bb8;--orange:#ff8a36;--purple:#9f68ff;--blue:#5aa7ff;--line:#30253f}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 20% 0,#25133a 0,transparent 34%),radial-gradient(circle at 80% 10%,#132649 0,transparent 28%),var(--bg);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:var(--text);min-height:100vh}
.wrap{width:min(980px,calc(100% - 28px));margin:0 auto;padding:28px 0 54px}.hero{display:flex;gap:16px;align-items:center;margin-bottom:22px}.fox{font-size:54px;filter:drop-shadow(0 0 14px #ff7a32)}h1{margin:0;font-size:34px}.sub{color:var(--muted);margin-top:5px}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.card{background:linear-gradient(145deg,rgba(27,20,40,.96),rgba(15,12,24,.96));border:1px solid var(--line);border-radius:18px;padding:18px;box-shadow:0 12px 34px #0007}.wide{grid-column:1/-1}.card h2{font-size:18px;margin:0 0 15px}.accent-orange{border-top:2px solid var(--orange)}.accent-purple{border-top:2px solid var(--purple)}.accent-blue{border-top:2px solid var(--blue)}.row{display:grid;grid-template-columns:150px 1fr auto;gap:12px;align-items:center;margin:12px 0}.label{font-size:14px;color:#d9cce4}.value{font-variant-numeric:tabular-nums;color:var(--muted);font-size:13px;min-width:38px;text-align:right}select,input[type=range],input[type=color],button{width:100%}select,button{background:#100c18;color:var(--text);border:1px solid #403151;border-radius:10px;padding:10px 12px;font-size:14px}input[type=color]{height:40px;border:1px solid #403151;border-radius:10px;padding:3px;background:#100c18}input[type=range]{accent-color:var(--purple)}.toggle{display:flex;gap:8px}.toggle button.active{border-color:var(--orange);box-shadow:0 0 0 1px var(--orange) inset;color:#fff}.big{padding:13px 16px;font-weight:700;background:linear-gradient(90deg,#7b45db,#d764bc,#ef7d35);border:0;cursor:pointer}.big.off{background:#17111f;border:1px solid #453655}.status{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.pill{background:#0d0a13;border:1px solid #2d2338;border-radius:12px;padding:11px}.pill b{display:block;font-size:12px;color:var(--muted);margin-bottom:4px}.pill span{font-size:14px}.toast{position:fixed;right:18px;bottom:18px;background:#171021;border:1px solid #714da0;border-radius:12px;padding:10px 14px;opacity:0;transform:translateY(8px);transition:.2s;pointer-events:none}.toast.show{opacity:1;transform:none}.hint{font-size:12px;color:var(--muted);margin-top:10px;line-height:1.5}
@media(max-width:720px){.grid{grid-template-columns:1fr}.wide{grid-column:auto}.row{grid-template-columns:110px 1fr auto}.status{grid-template-columns:repeat(2,1fr)}}
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

    <section class="card accent-purple">
      <h2>Panorama</h2>
      <button class="big" id="panorama">Show Fox Panorama</button>
      <div class="hint">Uses panorama files 100.bmp–105.bmp. The physical long-press toggle still works too.</div>
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
const $=id=>document.getElementById(id), patterns=['Dark','Test','Constant','Rainbow','Pulse','Breath'];
let state={};
function fillPatterns(el){el.innerHTML=patterns.map((p,i)=>`<option value="${i}">${p}</option>`).join('')}
fillPatterns($('tubePattern'));fillPatterns($('stripPattern'));
async function post(url,data){const body=new URLSearchParams(data);const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});if(!r.ok)throw new Error(await r.text());showToast();await loadState()}
function showToast(){const t=$('toast');t.classList.add('show');setTimeout(()=>t.classList.remove('show'),900)}
function active(id,on){$(id).classList.toggle('active',!!on)}
function apply(s){state=s;$('tubePattern').value=s.tube.pattern;$('tubeColor').value=s.tube.color;$('tubeIntensity').value=s.tube.intensity;$('tubeIntensityValue').textContent=s.tube.intensity+'/7';if(s.strip){$('stripPattern').value=s.strip.pattern;$('stripColor').value=s.strip.color;$('stripIntensity').value=s.strip.intensity;$('stripIntensityValue').textContent=s.strip.intensity+'/7'}else{$('stripCard').style.display='none'};$('face').innerHTML=Array.from({length:s.clock.faces},(_,i)=>`<option value="${i+1}">Face ${i+1}${i+1===8?' · Fox Den':''}</option>`).join('');$('face').value=s.clock.face;active('h12',s.clock.twelveHour);active('h24',!s.clock.twelveHour);active('zeroOn',!s.clock.blankZero);active('zeroOff',s.clock.blankZero);const p=$('panorama');p.textContent=s.panorama?'Return to Clock':'Show Fox Panorama';p.classList.toggle('off',s.panorama);$('ip').textContent=s.device.ip;$('rssi').textContent=s.device.rssi+' dBm';$('version').textContent=s.device.version}
async function loadState(){try{const r=await fetch('/api/state',{cache:'no-store'});if(r.ok)apply(await r.json())}catch(e){console.log(e)}}
$('tubePattern').onchange=e=>post('/api/tube',{pattern:e.target.value});$('tubeColor').onchange=e=>post('/api/tube',{color:e.target.value});$('tubeIntensity').oninput=e=>$('tubeIntensityValue').textContent=e.target.value+'/7';$('tubeIntensity').onchange=e=>post('/api/tube',{intensity:e.target.value});
$('stripPattern').onchange=e=>post('/api/strip',{pattern:e.target.value});$('stripColor').onchange=e=>post('/api/strip',{color:e.target.value});$('stripIntensity').oninput=e=>$('stripIntensityValue').textContent=e.target.value+'/7';$('stripIntensity').onchange=e=>post('/api/strip',{intensity:e.target.value});
$('face').onchange=e=>post('/api/clock',{face:e.target.value});$('h12').onclick=()=>post('/api/clock',{twelve:'1'});$('h24').onclick=()=>post('/api/clock',{twelve:'0'});$('zeroOn').onclick=()=>post('/api/clock',{blank:'0'});$('zeroOff').onclick=()=>post('/api/clock',{blank:'1'});$('panorama').onclick=()=>post('/api/panorama',{enabled:state.panorama?'0':'1'});
loadState();setInterval(loadState,15000);
</script>
</body>
</html>
)HTML";

WebUI::WebUI()
    : server(80), backlights(nullptr), tfts(nullptr), clock(nullptr),
      stored_config(nullptr), started(false), mdns_started(false)
{
}

void WebUI::begin(Backlights *backlights_, TFTs *tfts_, Clock *clock_, StoredConfig *stored_config_)
{
  backlights = backlights_;
  tfts = tfts_;
  clock = clock_;
  stored_config = stored_config_;

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

  // If Wi-Fi was not ready when begin() ran, start mDNS after reconnect.
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

void WebUI::handleState()
{
  String json;
  json.reserve(700);

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
