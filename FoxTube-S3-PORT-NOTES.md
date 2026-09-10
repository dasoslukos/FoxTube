# FoxTube ESP32-S3 Port Notes

_Last updated: 2026-09-10_

This branch ports EleksTubeHAX to the newer IPSTube ESP32-S3 hardware and adds several FoxTube-specific features.

## Target hardware

- ESP32-S3-WROOM-1U
- 8 MB flash
- No PSRAM
- Six ST7789 135×240 displays
- Direct per-display chip select GPIOs
- WS2812-compatible LED chain on GPIO42
- Optional 28-pixel bottom LED strip
- Single BOOT/menu button on GPIO0

## ESP32-S3 display port

Added an `IPSTube_S3` PlatformIO environment using:

- `esp32-s3-devkitc-1`
- 8 MB partition layout
- DIO flash mode
- 80 MHz flash
- USB CDC on boot

### TFT_eSPI ESP32-S3 fix

The vendored TFT_eSPI S3 processor header used Arduino's `FSPI` enum as the low-level register index. Under Arduino-ESP32 2.0.17 that resolves to `0`, but the TFT_eSPI S3 register macros require register index `2`.

The S3 processor definition was changed to:

```cpp
#define SPI_PORT 2
```

This fixed the `StoreProhibited` crash during TFT initialization.

## ESP32-S3 display pinout

| Function | GPIO |
|---|---:|
| TFT backlight / enable PWM | 9 |
| TFT reset | 4 |
| TFT DC / RS | 5 |
| TFT MOSI / SDA | 6 |
| TFT SCLK | 7 |
| RGB LED data | 42 |
| Menu / BOOT button | 0 |

Physical display chip-select order, left to right:

```text
GPIO15, GPIO16, GPIO17, GPIO18, GPIO8, GPIO3
```

HAX logical digit order is:

```text
0 = seconds ones
1 = seconds tens
2 = minutes ones
3 = minutes tens
4 = hours ones
5 = hours tens
```

The S3 chip-select mapping reverses that logical order so the clock reads correctly from left to right.

## Display dimming

GPIO9 is used as the hardware PWM control for display brightness.

The S3 build uses:

```cpp
#define DIM_WITH_ENABLE_PIN_PWM
#define DIM_SKIP_SOFTWARE_ALPHA
#define TFT_SKIP_REINIT
```

## Custom Fox Den clock face

A custom Fox Den clock face was added as face 8:

```text
80.bmp ... 89.bmp
```

`clockfaces.txt` includes:

```text
Fox Den
```

## Six-screen panorama mode

FoxTube can use all six displays as one effective:

```text
810 × 240
```

panorama.

Panorama files are stored as:

```text
100.bmp = leftmost panel
101.bmp
102.bmp
103.bmp
104.bmp
105.bmp = rightmost panel
```

Each panel is exactly:

```text
135 × 240
```

A long press of the single rear button while the normal clock is idle toggles panorama mode.

Normal clock updates continue internally while panorama mode is active but do not overwrite the artwork. Leaving panorama mode immediately restores the current time.

## Bottom LED strip support

The S3 board's WS2812-compatible chain is configured for:

```text
pixels 0–5   = six tube/display LEDs
pixels 6–33  = 28-pixel bottom strip
```

Total:

```text
34 LEDs
```

The PlatformIO S3 environment enables:

```text
HARDWAREMOD_IPSTUBE_CLOCK_WITH_LED_STRIPE
```

## Independent bottom-strip controls

The bottom strip has its own settings independent of the six tube LEDs:

- Pattern
- Color
- Brightness

Supported effects:

- Dark
- Test
- Constant
- Rainbow
- Pulse
- Breath

The bottom strip settings are stored separately in ESP32 Preferences/NVS under:

```text
foxstrip
```

This avoids changing the layout of the original EleksTubeHAX `StoredConfig` blob.

The one-button menu contains separate entries for:

```text
Bottom LED Pattern
Bottom LED Color
Bottom LED Intensity
```

## Local FoxTube Web UI

FoxTube now hosts a local browser interface over Wi-Fi.

Open:

```text
http://foxtube.local/
```

or the device's DHCP address.

The UI is embedded in firmware flash and does not use LittleFS.

Browser controls currently include:

### Tube LEDs

- Pattern
- Color
- Brightness

### Bottom strip

- Pattern
- Color
- Brightness

### Clock

- Clock face
- 12 / 24 hour mode
- Leading-zero behavior

### Panorama

- Show panorama
- Return to clock

### Status

- IP address
- mDNS hostname
- Wi-Fi RSSI
- Firmware version

The browser UI applies settings live and saves persistent settings to NVS.

## mDNS

FoxTube advertises:

```text
foxtube.local
```

on the local network.

## Build

```bash
pio run -e IPSTube_S3
```

Build LittleFS only when image/assets in `data/` change:

```bash
pio run -e IPSTube_S3 -t buildfs
```

## Firmware-only flash

```bash
esptool --port "$PORT" --baud 460800 write-flash \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size 8MB \
  0x10000 .pio/build/IPSTube_S3/IPSTube_S3_v1.3.13.bin
```

## Firmware + LittleFS flash

```bash
esptool --port "$PORT" --baud 460800 write-flash \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size 8MB \
  0x10000 .pio/build/IPSTube_S3/IPSTube_S3_v1.3.13.bin \
  0x140000 .pio/build/IPSTube_S3/littlefs.bin
```

Normal updates should not erase flash.

## Working state

Confirmed working on physical ESP32-S3 IPSTube hardware:

- ESP32-S3 boot
- EleksTubeHAX application startup
- TFT_eSPI / ST7789 initialization
- All six displays
- Correct digit order
- Wi-Fi
- NTP clock
- Custom Fox Den clock face
- Six-screen panorama
- Six tube LEDs
- 28-pixel bottom LED strip
- Independent bottom-strip settings
- One-button menu
- Local FoxTube Web UI
- `foxtube.local` mDNS

## Repository safety note

`tools/conv-bmp-to-clk.py` contains unrelated local work and should not be modified or reverted as part of the ESP32-S3/FoxTube changes.
