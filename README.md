# Thermal Imaging Simulation

> Real-time rendering of MLX90640 infrared sensor data at 320x240 with bilinear upscaling, target lock tracking, and a military-style SDL2 HUD.

## Overview

This project drives the MLX90640 32x24 IR array sensor on an ESP32 and renders its output as a live thermal display. The raw 768-pixel frame is upscaled 10x in both axes to 320x240 using bilinear interpolation, then passed through a target lock tracker that identifies the hottest point in the frame and a UI renderer that composites a HUD overlay. The SDL2 build replaces the Arduino runtime (millis, micros, Serial) with std::chrono equivalents so the full pipeline runs on desktop for development.

The rendering pipeline — colour mapping, interpolation kernel, blending — reflects a background in broadcast colour science and codec-level work rather than purely algorithmic thinking.

## System components

| Component | What it does |
|-----------|-------------|
| `MLX90640Driver` | I2C driver for the MLX90640 at address 0x33, 400kHz. Reads 832-word RAM frames, verifies checksum, converts raw data to float temperatures. Uses static buffers to avoid heap allocation on ESP32. Refresh rate configurable; default 16Hz. |
| `MLX90640Upscaler` | Expands 32x24 sensor output to 320x240 display resolution using bilinear interpolation: f(x,y) = (1-α)(1-β)·p00 + α(1-β)·p10 + (1-α)β·p01 + αβ·p11. A fixed-point Q16 alternative is available for tighter embedded loops. Nearest-neighbour and cubic modes also present; cubic is not recommended on ESP32. |
| `TargetLockTracker` | Scans the upscaled 320x240 frame for the hottest pixel. Runs a four-state machine (SEARCHING → LOCKING → LOCKED → LOST) with 0.5°C hysteresis to prevent flicker, exponential moving average smoothing (α=0.7), and a 50px maximum jump distance to filter noise between frames. |
| `ThermalUIRenderer` | SDL2 framebuffer renderer. Colour-maps temperature to a cold→hot palette (blue → cyan → yellow → red → white). Overlays a military-style HUD: corner brackets, centre crosshair, lock state indicator, and temperature panel. Reticle colour shifts green (searching) → yellow (locking) → red (locked). Night vision mode maps output to green monochrome. |
| `thermal_app_sdl2.cpp` | Desktop entry point. Stubs out the Arduino runtime using std::chrono and std::thread so the full sensor-to-display pipeline runs on macOS/Linux without hardware. |

## Tech stack

- C++17
- SDL2
- CMake
- PlatformIO
- MLX90640 (Melexis 32x24 IR array)
- ESP32 (esp32doit-devkit-v1 or ESP32-S3)

## Build

### Desktop (SDL2)

```bash
./build_sdl2_app.sh
# Requires clang++ and SDL2 (macOS: brew install sdl2)
# Output: build/bin/thermal_imaging_app
```

### ESP32 (PlatformIO)

```bash
# Build
platformio run

# Flash
platformio run --target upload

# Monitor
platformio device monitor --baud 115200
```

Default environment targets `esp32doit-devkit-v1`. For ESP32-S3:

```bash
platformio run -e esp32-s3
```

## Hardware

- **Sensor:** Melexis MLX90640 (32x24 IR array, ±1.5°C accuracy, up to 16Hz)
- **Microcontroller:** ESP32 (DevKit v1 or S3 variant)
- **Wiring:** SDA → GPIO 21, SCL → GPIO 22, I2C at 400kHz

## Background

Before moving into software, this project's author spent four years in broadcast delivery — DCI theatrical mastering, OTT pipeline engineering, colour grading pipelines at 28TB scale. The colour mapping and rendering decisions here come directly from that: the thermal palette is designed the way a colorist would think about a false-colour monitor scope, not just a gradient. The interpolation choices reflect the same tradeoffs that come up in image scaling at codec level.
