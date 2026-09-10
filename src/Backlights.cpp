#include "Backlights.h"

void Backlights::begin(StoredConfig::Config::Backlights *config_)
{
  Adafruit_NeoPixel::begin(); // Initialize RMT and pin
  config = config_;

  if (config->is_valid != StoredConfig::valid)
  {
    // Config is invalid, probably a new device never had its config written.
    // Load some reasonable defaults.
    Serial.println("Loaded Backlights config is invalid, using default.  This is normal on first boot.");
    setPattern(rainbow);
    setColorPhase(0);
    setIntensity(max_intensity - 1);
    setPulseRate(60);
    setBreathRate(20);
    setRainbowDuration(DEFAULT_BL_RAINBOW_DURATION_SEC);
    config->is_valid = StoredConfig::valid;
  }
  off = false;

#ifdef HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
  // We need per-zone brightness (tube LEDs vs bottom strip).  Leave the
  // NeoPixel library's global brightness at full scale and apply brightness
  // to each pixel color ourselves in loopSplitBacklights().
  Adafruit_NeoPixel::setBrightness(255);
  loadStripConfig();
#endif
}

#ifdef HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
void Backlights::loadStripConfig()
{
  strip_prefs.begin("foxstrip", false);

  const uint8_t saved_valid = strip_prefs.getUChar("valid", 0);
  if (saved_valid != StoredConfig::valid)
  {
    // First boot with independent strip support: mirror the current tube
    // settings so the clock looks exactly as it did before the split.
    strip_config.pattern = config->pattern;
    strip_config.color_phase = config->color_phase;
    strip_config.intensity = config->intensity;

    Serial.println("No saved bottom-strip config found; copying tube LED settings.");
    saveStripConfig();
    return;
  }

  strip_config.pattern = strip_prefs.getUChar("pattern", config->pattern);
  strip_config.color_phase = strip_prefs.getUShort("color_phase", config->color_phase);
  strip_config.intensity = strip_prefs.getUChar("intensity", config->intensity);

  // Sanity-check NVS values before using them as array indexes or levels.
  if (strip_config.pattern >= num_patterns)
    strip_config.pattern = config->pattern;
  if (strip_config.color_phase >= max_phase)
    strip_config.color_phase = config->color_phase % max_phase;
  if (strip_config.intensity >= max_intensity)
    strip_config.intensity = config->intensity % max_intensity;

  Serial.print("Loaded bottom-strip config: pattern=");
  Serial.print(patterns_str[strip_config.pattern]);
  Serial.print(", color=");
  Serial.printf("%06X", getStripColor());
  Serial.print(", intensity=");
  Serial.println(strip_config.intensity);
}

void Backlights::saveStripConfig()
{
  strip_prefs.putUChar("pattern", strip_config.pattern);
  strip_prefs.putUShort("color_phase", strip_config.color_phase);
  strip_prefs.putUChar("intensity", strip_config.intensity);
  strip_prefs.putUChar("valid", StoredConfig::valid);
}

void Backlights::setNextStripPattern(int8_t i)
{
  int8_t next_pattern = (strip_config.pattern + i) % num_patterns;
  while (next_pattern < 0)
  {
    next_pattern += num_patterns;
  }
  setStripPattern(patterns(next_pattern));
}

void Backlights::adjustStripColorPhase(int16_t adj)
{
  int16_t new_phase = (int16_t(strip_config.color_phase % max_phase) + adj) % max_phase;
  while (new_phase < 0)
  {
    new_phase += max_phase;
  }
  setStripColorPhase(new_phase);
}

void Backlights::setStripIntensity(uint8_t intensity)
{
  if (intensity >= max_intensity)
    intensity = max_intensity - 1;
  strip_config.intensity = intensity;
  pattern_needs_init = true;
}

void Backlights::adjustStripIntensity(int16_t adj)
{
  int16_t new_intensity = (int16_t(strip_config.intensity) + adj) % max_intensity;
  while (new_intensity < 0)
  {
    new_intensity += max_intensity;
  }
  setStripIntensity(uint8_t(new_intensity));
}
#endif

// These feel like they should be generalizable into a helper function.
// https://stackoverflow.com/questions/11720656/modulo-operation-with-negative-numbers
void Backlights::setNextPattern(int8_t i)
{
  int8_t next_pattern = (config->pattern + i) % num_patterns;
  while (next_pattern < 0)
  {
    next_pattern += num_patterns;
  }
  setPattern(patterns(next_pattern));
}

void Backlights::adjustColorPhase(int16_t adj)
{
  int16_t new_phase = (int16_t(config->color_phase % max_phase) + adj) % max_phase;
  while (new_phase < 0)
  {
    new_phase += max_phase;
  }
  setColorPhase(new_phase);
}

void Backlights::adjustIntensity(int16_t adj)
{
  int16_t new_intensity = (int16_t(config->intensity) + adj) % max_intensity;
  while (new_intensity < 0)
  {
    new_intensity += max_intensity;
  }
  setIntensity(new_intensity);
}

void Backlights::setIntensity(uint8_t intensity)
{
  config->intensity = intensity;
#ifdef HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
  // Global NeoPixel brightness would affect both zones.  Split-strip builds
  // apply brightness per pixel in loopSplitBacklights() instead.
#else
  setBrightness(0xFF >> max_intensity - config->intensity - 1);
#endif
  pattern_needs_init = true;
}

void Backlights::loop()
{
#ifdef HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
  loopSplitBacklights();
  pattern_needs_init = false;
  return;
#endif

  //   enum patterns { dark, test, constant, rainbow, pulse, breath, num_patterns };
  if (off || config->pattern == dark)
  {
    if (pattern_needs_init)
    {
      clear();
      show();
    }
  }
  else if (config->pattern == test)
  {
    testPattern();
  }
  else if (config->pattern == constant)
  {
    if (pattern_needs_init)
    {
      fill(phaseToColor(config->color_phase));
    }
    if (dimming)
    {
      setBrightness(0xFF >> max_intensity - BACKLIGHT_DIMMED_INTENSITY - 1);
    }
    else
    {
      setBrightness(0xFF >> max_intensity - config->intensity - 1);
    }
    show();
  }
  else if (config->pattern == rainbow)
  {
    rainbowPattern();
  }
  else if (config->pattern == pulse)
  {
    pulsePattern();
  }
  else if (config->pattern == breath)
  {
    breathPattern();
  }

  pattern_needs_init = false;
}

#ifdef HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
uint8_t Backlights::staticBrightnessForLevel(uint8_t intensity)
{
  if (intensity >= max_intensity)
    intensity = max_intensity - 1;

#ifdef BACKLIGHT_DIMMED_INTENSITY
  if (dimming)
  {
    if (BACKLIGHT_DIMMED_INTENSITY == 0)
      return 0;
    intensity = (uint8_t)BACKLIGHT_DIMMED_INTENSITY;
    if (intensity >= max_intensity)
      intensity = max_intensity - 1;
  }
#endif

  return uint8_t(0xFF >> (max_intensity - intensity - 1));
}

uint32_t Backlights::scaleColor(uint32_t color, uint8_t brightness)
{
  const uint8_t red = uint8_t((color >> 16) & 0xFF);
  const uint8_t green = uint8_t((color >> 8) & 0xFF);
  const uint8_t blue = uint8_t(color & 0xFF);

  const uint8_t scaled_red = uint8_t((uint16_t(red) * brightness) / 255);
  const uint8_t scaled_green = uint8_t((uint16_t(green) * brightness) / 255);
  const uint8_t scaled_blue = uint8_t((uint16_t(blue) * brightness) / 255);

  return (uint32_t(scaled_red) << 16) |
         (uint32_t(scaled_green) << 8) |
         uint32_t(scaled_blue);
}

void Backlights::renderZone(uint8_t first_pixel, uint8_t pixel_count,
                            patterns pattern, uint16_t color_phase,
                            uint8_t intensity)
{
  if (pixel_count == 0)
    return;

  if (pattern == dark)
  {
    for (uint8_t i = 0; i < pixel_count; i++)
      setPixelColor(first_pixel + i, 0);
    return;
  }

  if (pattern == test)
  {
    const uint8_t num_colors = 4;
    const uint16_t num_states = uint16_t(pixel_count) * num_colors;
    const uint16_t state = (millis() / test_ms_delay) % num_states;
    const uint8_t active_pixel = uint8_t(state / num_colors);
    const uint32_t color = 0xFF0000UL >> ((state % num_colors) * 8);
    const uint8_t brightness = staticBrightnessForLevel(intensity);

    for (uint8_t i = 0; i < pixel_count; i++)
    {
      setPixelColor(first_pixel + i,
                    i == active_pixel ? scaleColor(color, brightness) : 0);
    }
    return;
  }

  if (pattern == constant)
  {
    const uint32_t color =
        scaleColor(phaseToColor(color_phase), staticBrightnessForLevel(intensity));
    for (uint8_t i = 0; i < pixel_count; i++)
      setPixelColor(first_pixel + i, color);
    return;
  }

  if (pattern == rainbow)
  {
    uint32_t duration = uint32_t(round(getRainbowDuration() * 1000.0f));
    if (duration == 0)
      duration = 1;

    const uint16_t phase = uint16_t(
        round(float(millis() % duration) / float(duration) * max_phase)) % max_phase;

    // Spread a portion of the color wheel over each zone independently.
    uint16_t phase_per_pixel = (max_phase / pixel_count) / 3;
    if (phase_per_pixel == 0)
      phase_per_pixel = 1;

    const uint8_t brightness = staticBrightnessForLevel(intensity);
    for (uint8_t i = 0; i < pixel_count; i++)
    {
      const uint16_t my_phase = (phase + uint16_t(i) * phase_per_pixel) % max_phase;
      setPixelColor(first_pixel + i,
                    scaleColor(phaseToColor(my_phase), brightness));
    }
    return;
  }

  uint8_t effective_intensity = intensity;
#ifdef BACKLIGHT_DIMMED_INTENSITY
  if (dimming)
    effective_intensity = (uint8_t)BACKLIGHT_DIMMED_INTENSITY;
#endif
  if (effective_intensity >= max_intensity)
    effective_intensity = max_intensity - 1;

  if (pattern == pulse)
  {
    float pulse_length_millis = (60.0f * 1000.0f) / config->pulse_bpm;
    float brightness = 1.0f +
                       abs(sin(2.0f * M_PI * millis() / pulse_length_millis)) * 254.0f;
    brightness = brightness * effective_intensity / 7.0f;

    const uint32_t color =
        scaleColor(phaseToColor(color_phase), uint8_t(brightness));
    for (uint8_t i = 0; i < pixel_count; i++)
      setPixelColor(first_pixel + i, color);
    return;
  }

  if (pattern == breath)
  {
    float pulse_length_millis = (60.0f * 1000.0f) / config->breath_per_min;
    float brightness =
        (exp(sin(2.0f * M_PI * millis() / pulse_length_millis)) - 0.36787944f) * 108.0f;
    brightness = brightness * effective_intensity / 7.0f;

    uint8_t brightness_u8 = uint8_t(brightness);
    if (effective_intensity == 0)
      brightness_u8 = 0;
    else if (brightness_u8 < 1)
      brightness_u8 = 1;

    const uint32_t color =
        scaleColor(phaseToColor(color_phase), brightness_u8);
    for (uint8_t i = 0; i < pixel_count; i++)
      setPixelColor(first_pixel + i, color);
    return;
  }
}

void Backlights::loopSplitBacklights()
{
  if (off)
  {
    if (pattern_needs_init)
    {
      clear();
      show();
    }
    return;
  }

  // Pixels 0..5 are the six tube/display backlights.
  renderZone(0, 6, patterns(config->pattern), config->color_phase, config->intensity);

  // Pixels 6..33 are the 28-pixel bottom strip on this IPSTube variant.
  renderZone(6, NUM_BACKLIGHT_LEDS - 6, patterns(strip_config.pattern),
             strip_config.color_phase, strip_config.intensity);

  show();
}
#endif

void Backlights::pulsePattern()
{
  fill(phaseToColor(config->color_phase));

  float pulse_length_millis = (60.0f * 1000) / config->pulse_bpm;
  float val = 1 + abs(sin(2 * M_PI * millis() / pulse_length_millis)) * 254;
  if (dimming)
  {
    val = val * BACKLIGHT_DIMMED_INTENSITY / 7;
  }
  else
  {
    val = val * config->intensity / 7;
  }
  setBrightness((uint8_t)val);

  show();
}

void Backlights::breathPattern()
{
  fill(phaseToColor(config->color_phase));

  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  // Avoid a 0 value as it shuts off the LEDs and we have to re-initialize.
  float pulse_length_millis = (60.0f * 1000) / config->breath_per_min;
  float val = (exp(sin(2 * M_PI * millis() / pulse_length_millis)) - 0.36787944f) * 108.0f;

  if (dimming)
  {
    val = val * BACKLIGHT_DIMMED_INTENSITY / 7;
  }
  else
  {
    val = val * config->intensity / 7;
  }

  uint8_t brightness = (uint8_t)val;
  if (brightness < 1)
  {
    brightness = 1;
  }
  setBrightness(brightness);

  show();
}

void Backlights::testPattern()
{
  const uint8_t num_colors = 4; // or 3 if you don't want black
  uint8_t num_states = NUM_BACKLIGHT_LEDS * num_colors;
  uint8_t state = (millis() / test_ms_delay) % num_states;

  uint8_t digit = state / num_colors;
  uint32_t color = 0xFF0000 >> (state % num_colors) * 8;

  clear();
  setPixelColor(digit, color);

  if (dimming)
  {
    setBrightness(0xFF >> max_intensity - (uint8_t)BACKLIGHT_DIMMED_INTENSITY - 1);
  }
  else
  {
    setBrightness(0xFF >> max_intensity - config->intensity - 1);
  }

  show();
}

uint8_t Backlights::phaseToIntensity(uint16_t phase)
{
  uint16_t color = 0;
  if (phase <= 255)
  {
    // Ramping up
    color = phase;
  }
  else if (phase <= 511)
  {
    // Ramping down
    color = 511 - phase;
  }
  else
  {
    // Off
    color = 0;
  }
  if (color > 255)
  {
    // TODO: Trigger ERROR STATE, bug in code.
  }
  return uint8_t(color % 256);
}

uint32_t Backlights::phaseToColor(uint16_t phase)
{
  uint8_t red = phaseToIntensity(phase);
  uint8_t green = phaseToIntensity((phase + 256) % max_phase);
  uint8_t blue = phaseToIntensity((phase + 512) % max_phase);
  return (uint32_t(red) << 16 | uint32_t(green) << 8 | uint32_t(blue));
}

uint32_t Backlights::hueToPhase(float hue)
{
  hue = hue - 120.f;
  if (hue < 0)
  {
    hue = hue + 360.f;
  }
  uint32_t phase = uint32_t(round(768.f * (1.f - hue / 360.f)));
  phase = phase % max_phase;
  return (phase);
}

float Backlights::phaseToHue(uint32_t phase)
{
  float hue = 120.f + ((768.f - float(phase)) / 768.f) * 360.f;
  // h = 120 + (1 - p/768)*360
  if (hue >= 360.f)
  {
    hue = hue - 360.f;
  }
  return (round(hue));
}

void Backlights::rainbowPattern()
{
  // Divide by 3 to spread it out some, so the whole rainbow isn't displayed at once.
  // TODO Make this /3 a parameter
  const uint16_t phase_per_digit = (max_phase / NUM_BACKLIGHT_LEDS) / 3;

  // Rainbow roatation speed now configurable
  uint16_t duration = uint16_t(round(getRainbowDuration() * 1000));
  uint16_t phase = uint16_t(round(float(millis() % duration) / duration * max_phase));

  for (uint8_t digit = 0; digit < NUM_BACKLIGHT_LEDS; digit++)
  {
    // Shift the phase for this LED.
    uint16_t my_phase = (phase + digit * phase_per_digit) % max_phase;
    setPixelColor(digit, phaseToColor(my_phase));
  }
  if (dimming)
  {
#if BACKLIGHT_DIMMED_INTENSITY > 0
    setBrightness(0xFF >> max_intensity - (uint8_t)BACKLIGHT_DIMMED_INTENSITY - 1);
#else // turn off backlight if intensity is 0
    setBrightness(0);
#endif
  }
  else
  {
    setBrightness(0xFF >> max_intensity - config->intensity - 1);
  }
  show();
}

const String Backlights::patterns_str[Backlights::num_patterns] =
    {"Dark", "Test", "Constant", "Rainbow", "Pulse", "Breath"};
