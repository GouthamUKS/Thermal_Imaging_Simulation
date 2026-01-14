# Thermal Target Lock System: Technical Documentation

## Overview

The Target Lock Tracker is a sophisticated thermal targeting system designed for professional applications including defense, security, industrial monitoring, and rescue operations. It implements automatic hotspot detection with intelligent filtering to prevent lock flicker and jitter.

---

## Core Features

### 1. **Hotspot Detection**
- Scans entire 320×240 upscaled thermal array
- Identifies pixel with highest temperature (O(n) scan)
- Converts linear array index to X/Y display coordinates

### 2. **Hysteresis Logic**
- Prevents lock from jumping erratically between similar temperature pixels
- Configurable threshold (default: 0.5°C)
- Only releases lock if significantly cooler target found

### 3. **Position Smoothing**
- Exponential moving average filter
- Configurable smoothing factor (0.0 = raw, 1.0 = maximum)
- Reduces visual jitter in displayed reticle

### 4. **Confidence Tracking**
- Measures stability of locked target
- Analyzes temperature variance over time
- Colors reticle based on confidence level

### 5. **Reticle Rendering**
- Multiple styles: corner brackets, crosshair, circular, diamond
- Dynamic color based on lock state (green/yellow/red)
- Supports custom color override

---

## Architecture

### State Machine

```
┌─────────────┐
│  SEARCHING  │ ◄──── Initial state, no lock
└──────┬──────┘
       │ Find candidate
       │ (auto_tracking enabled)
       ▼
┌─────────────┐
│   LOCKING   │ ◄──── Building confidence on candidate
└──────┬──────┘
       │ Confidence threshold met
       │ (hysteresis satisfied)
       ▼
┌─────────────┐
│   LOCKED    │ ◄──── Active tracking of target
└──────┬──────┘
       │ Large temperature drop
       │ or invalid jump
       ▼
┌─────────────┐
│    LOST     │ ◄──── Target no longer visible
└──────┬──────┘
       │ New strong target found
       │ (temp > locked + threshold)
       ▼
    SEARCHING
```

### Processing Pipeline

```
┌─────────────────────────────────────────────────────────┐
│  Input: 320×240 upscaled thermal frame (float array)    │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │  Find Hottest Pixel    │
        │  (Linear scan O(n))    │
        └────────────┬───────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │  State Machine Update   │
        │  (SEARCH/LOCK/LOST)    │
        └────────────┬───────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │  Hysteresis Filtering   │
        │  (Prevent flicker)     │
        └────────────┬───────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │  Position Smoothing     │
        │  (Exponential MA)      │
        └────────────┬───────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │  Confidence Update      │
        │  (Stability analysis)  │
        └────────────┬───────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│  Output: TargetPoint with X/Y/Temp/Confidence           │
└─────────────────────────────────────────────────────────┘
```

---

## Algorithm Details

### Finding the Hottest Pixel

```cpp
// Linear scan of entire upscaled array
float hottest_temp = -999.0f;
uint32_t hottest_index = 0;

for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
    if (thermal_data[i] > hottest_temp) {
        hottest_temp = thermal_data[i];
        hottest_index = i;
    }
}

// Convert linear index to X/Y
uint32_t hottest_y = hottest_index / DISPLAY_WIDTH;    // 0-239
uint32_t hottest_x = hottest_index % DISPLAY_WIDTH;    // 0-319
```

**Complexity**: O(76,800) = O(1) on embedded system
**Time**: ~1-2 ms on ESP32 @ 240 MHz

### Index to Display Coordinates

For a 320×240 display:

```
Linear Index    Display Coordinates
─────────────   ──────────────────
0               (0, 0)      Top-left
1               (1, 0)
...
319             (319, 0)    Top-right
320             (0, 1)
...
76799           (319, 239)  Bottom-right

Formula:
y = index / 320
x = index % 320
```

**Example**: Index 1280
```
y = 1280 / 320 = 4
x = 1280 % 320 = 0
Result: (0, 4)  ← 4 rows down, leftmost
```

### Hysteresis Logic

**Purpose**: Prevent lock from jumping between pixels with nearly identical temperatures

**Algorithm**:

```cpp
bool shouldUpdateLock(float current_temp, float candidate_temp, float threshold) {
    float delta = candidate_temp - current_temp;
    
    // Strong increase → accept
    if (delta > threshold) {
        return true;  // New target significantly hotter
    }
    
    // Small change → accept (tracking)
    if (delta > -threshold) {
        return true;  // Within hysteresis band
    }
    
    // Large decrease → reject
    return false;  // Candidate significantly cooler
}
```

**Example** (threshold = 0.5°C):

```
Current lock:      45.2°C
Candidate 1:       45.7°C   (Δ = +0.5°C)   ✓ Accept (delta ≥ threshold)
Candidate 2:       45.0°C   (Δ = -0.2°C)   ✓ Accept (within band)
Candidate 3:       44.6°C   (Δ = -0.6°C)   ✗ Reject (delta < -threshold)
```

**Effect**: 
- Locks onto strongest target reliably
- Won't flip between nearly-equal pixels
- Allows smooth tracking of slowly-changing temperatures
- Still responds quickly to new hot sources

### Position Smoothing (EMA)

Reduces visual jitter in displayed reticle using exponential moving average:

$$x_{smoothed} = \alpha \cdot x_{current} + (1 - \alpha) \cdot x_{previous}$$

**Parameters**:
- $\alpha$ = smoothing factor (0.0 to 1.0)
  - 0.0 = no smoothing (raw, jittery)
  - 0.5 = medium smoothing
  - 1.0 = maximum smoothing (may lag)
- Default: 0.7 (good balance)

**Effect on reticle position**:

```
Raw (no smoothing):     Smoothed (α=0.7):
x: 154, 160, 155       x: 154.0 → 158.2 → 156.5
   [jittery]              [smooth curve]

Visual result: Reticle movements appear smoother,
less stutter and flicker
```

### Confidence Measurement

Analyzes temperature variance over 10 frames:

```cpp
1. Store max temp from last 10 frames
2. Calculate mean: mean = Σtemp / 10
3. Calculate variance: var = Σ(temp - mean)² / 10
4. Calculate std dev: σ = √var
5. Confidence: C = 1.0 - min(σ/5.0, 1.0)
   - σ = 0  → C = 1.0 (100%, very stable)
   - σ = 5  → C = 0.0 (0%, highly variable)
```

**Color mapping**:
- Confidence > 70% → Green (steady lock)
- Confidence < 70% → Yellow (fluctuating)
- Lost target → Red

---

## Display Coordinate Mapping

### Sensor to Display Scaling

MLX90640 sensors produce 32×24 data, which is upscaled 10× to 320×240:

```
Sensor Space                Display Space
(32×24)                     (320×240)

1×1 sensor pixel            10×10 display pixels

Hottest pixel at            Displays as:
sensor (15, 12)             Display (150-159, 120-129)

In upscaled array:
display_pixel[12*320 + 150] = hottest
                           ... (10×10 region)
display_pixel[12*320 + 159]
```

### No Coordinate Conversion Needed

Since thermal data is already upscaled before hotspot detection:

```cpp
// Find hottest in UPSCALED data
uint32_t hottest_index = findMaxIndex(upscaled_frame, 76800);

// Display coordinates directly
uint16_t display_y = hottest_index / DISPLAY_WIDTH;
uint16_t display_x = hottest_index % DISPLAY_WIDTH;

// These X/Y can be used directly in reticle drawing
drawReticleAt(display_x, display_y);
```

No inverse scaling needed—coordinates already in display space!

---

## Reticle Styles

### 1. Corner Brackets (Military/Default)

```
Classic targeting reticle with corner brackets:

    +---+
    |
    |
 +--+
          Target
           ▼
          ⊕
 +--+
    |
    |
    +---+
```

**Code**:
```cpp
tracker.setReticleStyle(ReticleStyle::CORNER_BRACKETS);
// Size: 40 pixels
// Corner length: 12 pixels
// Thickness: 2 pixels
```

**Best for**: Professional thermal imaging, defense applications

---

### 2. Crosshair

```
Simple crosshair targeting:

       │
    ───┼───
       │
```

**Code**:
```cpp
tracker.setReticleStyle(ReticleStyle::CROSSHAIR);
```

**Best for**: Fast-moving targets, simple UI

---

### 3. Circular Ring

```
Circular reticle:

      ╱───╲
     ╱     ╲
    │   ⊕   │
     ╲     ╱
      ╲───╱
```

**Code**:
```cpp
tracker.setReticleStyle(ReticleStyle::CIRCULAR);
```

**Best for**: Pursuit/evasion scenarios

---

### 4. Diamond

```
Diamond shape:

      ╱╲
     ╱  ╲
    │    │
     ╲  ╱
      ╲╱
```

**Code**:
```cpp
tracker.setReticleStyle(ReticleStyle::DIAMOND);
```

**Best for**: Modern UI, distinctive appearance

---

## Usage Examples

### Basic Usage

```cpp
// Initialize system
TargetLockTracker tracker;

// Configure
tracker.setReticleStyle(ReticleStyle::CORNER_BRACKETS);
tracker.setHysteresisThreshold(0.5f);  // 0.5°C
tracker.setSmoothingFactor(0.7f);      // 70% smoothing

// Process frames
while (true) {
    // Capture and upscale thermal data
    upscaler.upscale(raw_frame, upscaled_frame);
    
    // Update target lock
    TargetPoint target = tracker.processThermalFrame(upscaled_frame);
    
    // Draw reticle
    tracker.drawReticle(display_buffer);
    
    // Update display
    display.updateBuffer(display_buffer);
}
```

### Manual Lock Override

```cpp
// User manually selects target
TargetPoint manual_target;
manual_target.x = 160;      // Center of screen
manual_target.y = 120;
manual_target.temperature = 95.5f;

// Acquire lock
tracker.manualLock(160, 120, 95.5f);

// Later: release
tracker.releaseLock();
```

### Multiple Target Tracking

```cpp
// Find top 3 hottest pixels
HotPixel targets[3];
findMultipleHotspots(upscaled_frame, targets, 3);

// Display all targets
for (int i = 0; i < 3; ++i) {
    if (targets[i].temperature > -999.0f) {
        drawTargetMarker(targets[i].x, targets[i].y);
    }
}
```

### Confidence-Based Display

```cpp
LockStatistics stats = tracker.getStatistics(upscaled_frame);

if (stats.is_stable) {
    Serial.println("Lock: STABLE");
} else {
    Serial.println("Lock: UNSTABLE - moving target");
}
```

---

## Performance Characteristics

| Operation | Time | FPS Impact |
|-----------|------|-----------|
| Hotspot scan (76,800 px) | 1-2 ms | Negligible |
| Hysteresis eval | <0.1 ms | Negligible |
| Smoothing filter | <0.1 ms | Negligible |
| Reticle drawing | 1-2 ms | ~2% CPU |
| **Total tracking** | **2-4 ms** | **~4-5% CPU** |

**Result**: Tracking adds <5% CPU overhead, easily fits within real-time constraints.

---

## Configuration Guide

### For Stable Locks (Stationary Targets)

```cpp
tracker.setHysteresisThreshold(0.2f);   // Tight: 0.2°C
tracker.setSmoothingFactor(0.9f);       // High: 90% smoothing
tracker.setMaxJumpDistance(10.0f);      // Tight: 10 pixels
```

**Effect**: Very stable, almost no flicker. May lag on fast motion.

---

### For Responsive Tracking (Moving Targets)

```cpp
tracker.setHysteresisThreshold(1.0f);   // Loose: 1.0°C
tracker.setSmoothingFactor(0.4f);       // Low: 40% smoothing
tracker.setMaxJumpDistance(100.0f);     // Loose: 100 pixels
```

**Effect**: Follows quickly, may jitter on stationary targets.

---

### For Balanced Performance (Default)

```cpp
tracker.setHysteresisThreshold(0.5f);   // 0.5°C
tracker.setSmoothingFactor(0.7f);       // 70% smoothing
tracker.setMaxJumpDistance(50.0f);      // 50 pixels
```

**Effect**: Good balance of stability and responsiveness.

---

## Defense/Security Applications

### Perimeter Monitoring
```cpp
// Track all intrusions
tracker.setAutoTracking(true);
// Log all targets for post-event analysis
logTargetTrajectory(target);
```

### Threat Assessment
```cpp
// Higher confidence = higher threat level
uint8_t threat_level = (uint8_t)(confidence * 100);

if (threat_level > 80) {
    activateAlarm();
}
```

### Multi-Target Engagement
```cpp
// Prioritize by temperature and distance
HotPixel threats[NUM_THREATS];
findMultipleHotspots(thermal_data, threats, NUM_THREATS);
sortByThreatLevel(threats);
engageHighestThreat(threats[0]);
```

---

## Troubleshooting

### Lock Flickers Between Similar Pixels

**Symptom**: Reticle jumps between pixels of nearly identical temperature

**Solution**:
```cpp
// Increase hysteresis threshold
tracker.setHysteresisThreshold(1.0f);  // Was 0.5f

// Or increase smoothing
tracker.setSmoothingFactor(0.9f);      // Was 0.7f
```

---

### Reticle Lags Behind Moving Target

**Symptom**: Reticle trails behind fast-moving thermal source

**Solution**:
```cpp
// Reduce smoothing
tracker.setSmoothingFactor(0.3f);  // Was 0.7f

// Or increase max jump distance
tracker.setMaxJumpDistance(150.0f);  // Was 50.0f
```

---

### Lock Lost on Scene Changes

**Symptom**: Lock drops when entire scene temperature changes

**Solution**:
```cpp
// This is expected behavior—new baseline established
// Re-acquire after 1-2 seconds
// Or manually lock if needed
tracker.manualLock(x, y, temp);
```

---

## References

- Thermal imaging fundamentals (FLIR documentation)
- Real-time tracking algorithms (computer vision)
- State machine design patterns
- Exponential moving average (signal processing)

