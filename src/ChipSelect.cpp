#include "ChipSelect.h"
#include <driver/gpio.h>

#ifdef HARDWARE_MARVELTUBESMINI_CLOCK

static bool i2cWriteReg(uint8_t address, uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void ChipSelect::i2cReplayInitSequence(uint8_t address)
{
  static bool replay_done = false;
  if (replay_done)
    return;

  replay_done = true;

  Serial.println("ChipSelect: I2C expander init sequence 01 FE -> 01 FC -> 01 FE");

  i2cWriteReg(address, 0x01, 0xFE);
  delay(90);

  i2cWriteReg(address, 0x01, 0xFC);
  delay(90);

  i2cWriteReg(address, 0x01, 0xFE);
  delay(100);
}

void ChipSelect::begin()
{
  Serial.println("ChipSelect::begin (MarvelTubesMini / I2C expander)");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setTimeOut(50);

  Wire.beginTransmission(EXPANDER_ADDR);
  bool expander_present = (Wire.endTransmission() == 0);

  Serial.printf(
      "Expander present at 0x%02X: %s\n",
      EXPANDER_ADDR,
      expander_present ? "yes" : "no");

  if (expander_present)
  {
    i2cReplayInitSequence(EXPANDER_ADDR);
    setDim(150);

    Serial.println("ChipSelect: I2C expander init done.");
  }
  else
  {
    Serial.println(
        "ChipSelect: I2C expander not found! "
        "CHECK WIRING AND DO AN I2C SCAN!");
  }
}

void ChipSelect::clear(bool update_)
{
  i2cWriteReg(
      EXPANDER_ADDR,
      EXPANDER_CMD_DIGIT,
      0xFF); // all CS high = all deselected
}

void ChipSelect::setAll(bool update_)
{
  i2cWriteReg(
      EXPANDER_ADDR,
      EXPANDER_CMD_DIGIT,
      0x00); // all CS low = all selected
}

void ChipSelect::setDigit(uint8_t digit, bool update_)
{
  i2cWriteReg(
      EXPANDER_ADDR,
      EXPANDER_CMD_DIGIT,
      cs_masks[digit]);
}

void ChipSelect::setDim(uint32_t duty)
{
  i2cWriteReg(
      EXPANDER_ADDR,
      EXPANDER_CMD_DIM,
      (uint8_t)(duty & 0xFF));
}

void ChipSelect::update()
{
  // no-op: I2C expander writes are immediate
}

void ChipSelect::reclaimPins()
{
  // no-op for MarvelTubesMini
}

bool ChipSelect::isSecondsOnes()
{
  return true;
}

bool ChipSelect::isSecondsTens()
{
  return true;
}

bool ChipSelect::isMinutesOnes()
{
  return true;
}

bool ChipSelect::isMinutesTens()
{
  return true;
}

bool ChipSelect::isHoursOnes()
{
  return true;
}

bool ChipSelect::isHoursTens()
{
  return true;
}

void ChipSelect::enableAllCSPins()
{
}

void ChipSelect::disableAllCSPins()
{
}

void ChipSelect::enableDigitCSPins(uint8_t digit)
{
}

void ChipSelect::disableDigitCSPins(uint8_t digit)
{
}

void ChipSelect::setEnabled(bool enabled)
{
}

#else // !HARDWARE_MARVELTUBESMINI_CLOCK


// -----------------------------------------------------------------------------
// Original IPSTube
// -----------------------------------------------------------------------------

#ifdef HARDWARE_IPSTUBE_CLOCK

// Define the pins for each LCD's enable wire.
//
// The order is from left to right, so the first pin is for the seconds ones,
// the last for the hours tens.
//
// LCD2 is the leftmost one         - seconds one - pin 21 as GPIO15
// LCD3 is the second from the left - seconds ten - pin 22 as GPIO2
// LCD4 is the third from the left  - minutes one - pin 23 as GPIO27
// LCD5 is the fourth from the left - minutes ten - pin 17 as GPIO14
// LCD6 is the fifth from the left  - hours one   - pin 18 as GPIO12
// LCD7 is the rightmost one        - hours ten   - pin 20 as GPIO13

const int lcdEnablePins[NUM_DIGITS] = {
    GPIO_NUM_15,
    GPIO_NUM_2,
    GPIO_NUM_27,
    GPIO_NUM_14,
    GPIO_NUM_12,
    GPIO_NUM_13
};

const int numLCDs = NUM_DIGITS;

#endif // HARDWARE_IPSTUBE_CLOCK


// -----------------------------------------------------------------------------
// IPSTube ESP32-S3
// -----------------------------------------------------------------------------

#ifdef HARDWARE_IPSTUBE_S3_CLOCK

/*
 * ESP32-S3 IPSTube display chip-select mapping.
 *
 * Physical screen order, LEFT -> RIGHT:
 *
 *   GPIO15  GPIO16  GPIO17  GPIO18  GPIO8  GPIO3
 *
 * HAX does NOT index lcdEnablePins[] by physical left-to-right position.
 * Its logical digit order is:
 *
 *   index 0 = seconds ones
 *   index 1 = seconds tens
 *   index 2 = minutes ones
 *   index 3 = minutes tens
 *   index 4 = hours ones
 *   index 5 = hours tens
 *
 * Since seconds ones is the RIGHTMOST physical display on this S3 board,
 * the physical CS sequence must therefore be reversed here.
 *
 * Logical display order:
 *
 *   Seconds Ones  -> GPIO3
 *   Seconds Tens  -> GPIO8
 *   Minutes Ones  -> GPIO18
 *   Minutes Tens  -> GPIO17
 *   Hours Ones    -> GPIO16
 *   Hours Tens    -> GPIO15
 */

const int lcdEnablePins[NUM_DIGITS] = {
    GPIO_NUM_3,   // seconds ones - rightmost
    GPIO_NUM_8,   // seconds tens
    GPIO_NUM_18,  // minutes ones
    GPIO_NUM_17,  // minutes tens
    GPIO_NUM_16,  // hours ones
    GPIO_NUM_15   // hours tens - leftmost
};

const int numLCDs = NUM_DIGITS;

#endif // HARDWARE_IPSTUBE_S3_CLOCK


// -----------------------------------------------------------------------------
// MarvelTubes
// -----------------------------------------------------------------------------

#ifdef HARDWARE_MARVELTUBES_CLOCK

// Define the pins for each LCD's enable wire
// (left to right: hours tens .. seconds ones)

const int lcdEnablePins[NUM_DIGITS] = {
    15,
    33,
    34,
    35,
    36,
    37
};

const int numLCDs = NUM_DIGITS;

#endif // HARDWARE_MARVELTUBES_CLOCK


// -----------------------------------------------------------------------------
// MarvelTubes Gen2
// -----------------------------------------------------------------------------

#ifdef HARDWARE_MARVELTUBES_GEN2_CLOCK

// Direct CS GPIO lines for MarvelTubes Gen2
// order:
//   seconds ones,
//   seconds tens,
//   minutes ones,
//   minutes tens,
//   hours ones,
//   hours tens
//
// GPIO14 = Seconds Ones (rightmost)
// GPIO27 = Seconds Tens
// GPIO17 = Minutes Ones
// GPIO5  = Minutes Tens
// GPIO19 = Hours Ones
// GPIO22 = Hours Tens (leftmost)

const int lcdEnablePins[NUM_DIGITS] = {
    GPIO_NUM_14,
    GPIO_NUM_27,
    GPIO_NUM_17,
    GPIO_NUM_5,
    GPIO_NUM_19,
    GPIO_NUM_22
};

const int numLCDs = NUM_DIGITS;

#endif // HARDWARE_MARVELTUBES_GEN2_CLOCK


// -----------------------------------------------------------------------------
// D'Esign Clock
// -----------------------------------------------------------------------------

#ifdef HARDWARE_DESIGN_CLOCK

// Direct CS GPIO lines
// order:
//   seconds ones,
//   seconds tens,
//   minutes ones,
//   minutes tens,
//   hours ones,
//   hours tens
//
// GPIO25 = Seconds Ones (rightmost)
// GPIO26 = Seconds Tens
// GPIO12 = Minutes Ones
// GPIO14 = Minutes Tens
// GPIO18 = Hours Ones
// GPIO17 = Hours Tens (leftmost)

const int lcdEnablePins[NUM_DIGITS] = {
    GPIO_NUM_25,
    GPIO_NUM_26,
    GPIO_NUM_12,
    GPIO_NUM_14,
    GPIO_NUM_18,
    GPIO_NUM_17
};

const int numLCDs = NUM_DIGITS;

#endif // HARDWARE_DESIGN_CLOCK


void ChipSelect::begin()
{
#ifndef CS_DIRECT_GPIO

  pinMode(CSSR_LATCH_PIN, OUTPUT);
  pinMode(CSSR_DATA_PIN, OUTPUT);
  pinMode(CSSR_CLOCK_PIN, OUTPUT);

#ifdef HARDWARE_XUNFENG_CLOCK

  // Xunfeng clock shift register seems to need some pins to be set
  // LOW or HIGH at the beginning, otherwise it will not work correctly.

  pinMode(GPIO_NUM_14, OUTPUT);
  pinMode(GPIO_NUM_17, OUTPUT);

  digitalWrite(GPIO_NUM_14, LOW);
  digitalWrite(GPIO_NUM_17, HIGH);

#endif // HARDWARE_XUNFENG_CLOCK

  digitalWrite(CSSR_DATA_PIN, LOW);
  digitalWrite(CSSR_CLOCK_PIN, LOW);
  digitalWrite(CSSR_LATCH_PIN, LOW);

  update();

#else // CS_DIRECT_GPIO

  // Initialize all six display CS pins as outputs and leave every
  // display deselected initially.

  for (int i = 0; i < numLCDs; ++i)
  {
#ifdef DEBUG_OUTPUT_CHIPSELECT
    Serial.print("ChipSelect::begin - Config pin ");
    Serial.print(lcdEnablePins[i]);
    Serial.println(" as OUTPUT, default disabled");
#endif

    gpio_reset_pin(
        static_cast<gpio_num_t>(lcdEnablePins[i]));

    pinMode(
        lcdEnablePins[i],
        OUTPUT);

    digitalWrite(
        lcdEnablePins[i],
        DIGIT_CS_INACTIVE_LEVEL);
  }

#endif // CS_DIRECT_GPIO
}


void ChipSelect::clear(bool update_)
{
#ifndef CS_DIRECT_GPIO

  setDigitMap(
      all_off,
      update_);

#else

  disableAllCSPins();

#endif
}


void ChipSelect::setAll(bool update_)
{
#ifndef CS_DIRECT_GPIO

  setDigitMap(
      all_on,
      update_);

#else

  enableAllCSPins();

#endif
}


void ChipSelect::reclaimPins()
{
#ifdef CS_DIRECT_GPIO

  for (int i = 0; i < numLCDs; ++i)
  {
    gpio_reset_pin(
        static_cast<gpio_num_t>(lcdEnablePins[i]));

    pinMode(
        lcdEnablePins[i],
        OUTPUT);

    digitalWrite(
        lcdEnablePins[i],
        DIGIT_CS_INACTIVE_LEVEL);
  }

#endif
}


void ChipSelect::setDigit(uint8_t digit, bool update_)
{
#ifndef CS_DIRECT_GPIO

  // Set the bit for the given digit in the digits_map.
  setDigitMap(
      1 << digit,
      update_);

  if (update_)
    update();

#else

  /*
   * Direct GPIO CS implementation.
   *
   * Deactivate the previously selected display,
   * remember the new display,
   * then activate its CS line.
   *
   * No update() call here because the display must remain selected
   * while TFT_eSPI performs its SPI writes.
   */

  disableDigitCSPins(currentLCD);

  currentLCD = digit;

  enableDigitCSPins(digit);

#endif
}


void ChipSelect::update()
{
#ifndef CS_DIRECT_GPIO

  /*
   * Documented in README.md.
   *
   * Q7 and Q6 are unused.
   * Q5 is Seconds Ones.
   * Q0 is Hours Tens.
   *
   * Q7 is the first bit written and Q0 is the last.
   * Push two dummy bits, then Seconds Ones through Hours Tens.
   *
   * CS is active-low, whereas digits_map uses 1 = enabled and
   * 0 = disabled, so invert the map first.
   */

  uint8_t to_shift = (~digits_map) << 2;

  digitalWrite(
      CSSR_LATCH_PIN,
      LOW);

  shiftOut(
      CSSR_DATA_PIN,
      CSSR_CLOCK_PIN,
      LSBFIRST,
      to_shift);

  digitalWrite(
      CSSR_LATCH_PIN,
      HIGH);

#else

  /*
   * Direct GPIO CS:
   *
   * setDigit() has already selected the correct display.
   * Keep its CS asserted while TFT_eSPI writes to it.
   */

  digitalWrite(
      lcdEnablePins[currentLCD],
      DIGIT_CS_ACTIVE_LEVEL);

#endif
}


bool ChipSelect::isSecondsOnes()
{
#ifndef CS_DIRECT_GPIO

  return (
      (digits_map & SECONDS_ONES_MAP) > 0);

#else

  return true;

#endif
}


bool ChipSelect::isSecondsTens()
{
#ifndef CS_DIRECT_GPIO

  return (
      (digits_map & SECONDS_TENS_MAP) > 0);

#else

  return true;

#endif
}


bool ChipSelect::isMinutesOnes()
{
#ifndef CS_DIRECT_GPIO

  return (
      (digits_map & MINUTES_ONES_MAP) > 0);

#else

  return true;

#endif
}


bool ChipSelect::isMinutesTens()
{
#ifndef CS_DIRECT_GPIO

  return (
      (digits_map & MINUTES_TENS_MAP) > 0);

#else

  return true;

#endif
}


bool ChipSelect::isHoursOnes()
{
#ifndef CS_DIRECT_GPIO

  return (
      (digits_map & HOURS_ONES_MAP) > 0);

#else

  return true;

#endif
}


bool ChipSelect::isHoursTens()
{
#ifndef CS_DIRECT_GPIO

  return (
      (digits_map & HOURS_TENS_MAP) > 0);

#else

  return true;

#endif
}


void ChipSelect::enableAllCSPins()
{
#ifdef CS_DIRECT_GPIO

  for (int i = 0; i < numLCDs; ++i)
  {
    digitalWrite(
        lcdEnablePins[i],
        DIGIT_CS_ACTIVE_LEVEL);
  }

#endif
}


void ChipSelect::disableAllCSPins()
{
#ifdef CS_DIRECT_GPIO

  for (int i = 0; i < numLCDs; ++i)
  {
    digitalWrite(
        lcdEnablePins[i],
        DIGIT_CS_INACTIVE_LEVEL);
  }

#endif
}


void ChipSelect::enableDigitCSPins(uint8_t digit)
{
#ifdef CS_DIRECT_GPIO

  digitalWrite(
      lcdEnablePins[digit],
      DIGIT_CS_ACTIVE_LEVEL);

#endif
}


void ChipSelect::disableDigitCSPins(uint8_t digit)
{
#ifdef CS_DIRECT_GPIO

  digitalWrite(
      lcdEnablePins[digit],
      DIGIT_CS_INACTIVE_LEVEL);

#endif
}


void ChipSelect::setEnabled(bool enabled)
{
#if defined(DIM_WITH_ENABLE_PIN_PWM)

  // Hardware PWM dimming is handled by
  // ProcessUpdatedDimming()/ledcWrite().
  (void)enabled;

#elif defined(TFT_ENABLE_PIN) && TFT_ENABLE_PIN >= 0

  digitalWrite(
      TFT_ENABLE_PIN,
      enabled
          ? ACTIVATEDISPLAYS
          : DEACTIVATEDISPLAYS);

#else

  // Hardware without an explicit enable pin.
  (void)enabled;

#endif
}


void ChipSelect::setDim(uint32_t duty)
{
  /*
   * No-op for the direct GPIO clock variants.
   *
   * Software alpha dimming is used for non-MarvelTubesMini variants
   * unless the hardware provides its own PWM implementation.
   */

  (void)duty;
}


#endif // !HARDWARE_MARVELTUBESMINI_CLOCK