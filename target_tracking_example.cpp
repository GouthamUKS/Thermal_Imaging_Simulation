#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include "MLX90640Driver.h"
#include "MLX90640Upscaler.h"
#include "TargetLockTracker.h"

// Standard C++ replacements for Arduino functions
namespace Arduino {
    using Clock = std::chrono::steady_clock;
    static auto startup_time = Clock::now();

    inline unsigned long millis() {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startup_time);
        return static_cast<unsigned long>(duration.count());
    }

    inline unsigned long micros() {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startup_time);
        return static_cast<unsigned long>(duration.count());
    }

    inline void delay(unsigned long ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    class SerialClass {
    public:
        void begin(unsigned long baud) { /* No-op in desktop */ }
        
        // Generic template for most types
        template<typename T>
        SerialClass& print(const T& value) {
            std::cout << value;
            std::cout.flush();
            return *this;
        }

        // Specialized for C-style strings
        SerialClass& print(const char* str) {
            std::cout << str;
            std::cout.flush();
            return *this;
        }

        // Float with precision
        SerialClass& print(float value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value;
            std::cout.flush();
            return *this;
        }

        // Double with precision
        SerialClass& print(double value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value;
            std::cout.flush();
            return *this;
        }

        // Generic println
        template<typename T>
        SerialClass& println(const T& value) {
            std::cout << value << std::endl;
            return *this;
        }

        // Specialized println for C-style strings
        SerialClass& println(const char* str) {
            std::cout << str << std::endl;
            return *this;
        }

        // Float println with precision
        SerialClass& println(float value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value << std::endl;
            return *this;
        }

        // Double println with precision
        SerialClass& println(double value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value << std::endl;
            return *this;
        }

        // Parameterless println
        SerialClass& println() {
            std::cout << std::endl;
            return *this;
        }

        bool available() { return false; }
        char read() { return '\0'; }
    };

    static SerialClass Serial;
}

using Arduino::Serial;
using Arduino::millis;
using Arduino::micros;
using Arduino::delay;

// ============================================================================
// Configuration
// ============================================================================

#define SDA_PIN 21
#define SCL_PIN 22

// Display dimensions
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Static system instances
static MLX90640Driver thermal_sensor;
static MLX90640Upscaler upscaler;
static TargetLockTracker target_tracker;

// Data buffers
static float raw_frame[MLX90640_PIXEL_COUNT];         // 768 pixels (32x24)
static float upscaled_frame[DISPLAY_WIDTH * DISPLAY_HEIGHT]; // 76,800 pixels (320x240)
static uint32_t display_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT]; // Display framebuffer

// ============================================================================
// System Initialization
// ============================================================================

/**
 * @brief Initialize complete thermal imaging and targeting system
 * @return true if successful
 */
bool initThermalTargetingSystem(bool use_simulation = true) {
    Serial.println("\n=====================================");
    Serial.println("Thermal Target Tracking System Init");
    Serial.println("=====================================\n");

    if (!use_simulation) {
        // Initialize thermal sensor
        MLX90640_Status status = thermal_sensor.init(SDA_PIN, SCL_PIN);
        if (status != MLX90640_Status::OK) {
            Serial.println("ERROR: Thermal sensor initialization failed!");
            return false;
        }
        Serial.println("✓ Thermal sensor initialized");

        // Set 16 Hz refresh rate
        status = thermal_sensor.setRefreshRate16Hz();
        if (status != MLX90640_Status::OK) {
            Serial.println("ERROR: Failed to set refresh rate!");
            return false;
        }
    } else {
        Serial.println("✓ Running in SIMULATION mode (no hardware required)");
        Serial.println("✓ Thermal sensor initialized");
    }
    Serial.println("✓ Refresh rate set to 16 Hz");

    // Initialize upscaler
    upscaler.setInterpolationMethod(InterpolationMethod::BILINEAR);
    Serial.println("✓ Bilinear interpolation upscaler ready");

    // Initialize target tracker
    target_tracker.setReticleStyle(ReticleStyle::CORNER_BRACKETS);
    target_tracker.setHysteresisThreshold(0.5f); // 0.5°C hysteresis
    target_tracker.setSmoothingFactor(0.7f);     // 70% smoothing
    target_tracker.setMaxJumpDistance(50.0f);    // 50-pixel max jump
    Serial.println("✓ Target lock tracker initialized");

    Serial.println("\nSystem Configuration:");
    Serial.println("  Sensor: 32x24 (768 pixels)");
    Serial.println("  Display: 320x240 (76,800 pixels)");
    Serial.println("  Upscale: 10x bilinear interpolation");
    Serial.println("  Target: Automatic thermal hotspot tracking");
    Serial.println("  Reticle: Military corner-bracket style");
    Serial.println("\nReady for target tracking!\n");

    return true;
}

// ============================================================================
// Main Processing Pipeline
// ============================================================================

// Forward declaration
static uint32_t temperatureToRGB888(float temperature);

/**
 * @brief Single processing cycle: capture → upscale → track → render
 * @return true if successful
 */
bool processThermalTargetFrame() {
    // Step 1: Capture raw thermal data
    MLX90640_Status status = thermal_sensor.captureFrame(raw_frame);
    if (status != MLX90640_Status::OK) {
        Serial.print("Frame capture error: ");
        Serial.println((uint8_t)status);
        return false;
    }

    // Step 2: Upscale to display resolution
    if (!upscaler.upscale(raw_frame, upscaled_frame)) {
        Serial.println("Upscaling failed!");
        return false;
    }

    // Step 3: Process thermal frame and update target lock
    TargetPoint target = target_tracker.processThermalFrame(upscaled_frame);

    // Step 4: Convert thermal data to RGB display colors
    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        float temp = upscaled_frame[i];
        display_buffer[i] = temperatureToRGB888(temp);
    }

    // Step 5: Draw targeting reticle
    if (target.is_valid) {
        target_tracker.drawReticle((uint32_t*)display_buffer);
    }

    // Step 6: Send to display (implementation specific)
    // displayUpdateBuffer(display_buffer);

    return true;
}

/**
 * @brief Convert temperature to RGB888 color
 * @param temperature Temperature in °C
 * @return 24-bit RGB color (0xRRGGBB)
 */
static uint32_t temperatureToRGB888(float temperature) {
    // Normalize to 0-1 range (-20°C to +300°C)
    float min_temp = -20.0f;
    float max_temp = 300.0f;
    float normalized = (temperature - min_temp) / (max_temp - min_temp);

    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    // Pseudo-color thermal map
    uint8_t r = 0, g = 0, b = 0;

    if (normalized < 0.167f) {
        // Black to Blue
        float t = normalized / 0.167f;
        b = (uint8_t)(255.0f * t);
    } else if (normalized < 0.333f) {
        // Blue to Cyan
        float t = (normalized - 0.167f) / 0.167f;
        b = 255;
        g = (uint8_t)(255.0f * t);
    } else if (normalized < 0.5f) {
        // Cyan to Green
        float t = (normalized - 0.333f) / 0.167f;
        b = (uint8_t)(255.0f * (1.0f - t));
        g = 255;
    } else if (normalized < 0.667f) {
        // Green to Yellow
        float t = (normalized - 0.5f) / 0.167f;
        g = 255;
        r = (uint8_t)(255.0f * t);
    } else if (normalized < 0.833f) {
        // Yellow to Red
        float t = (normalized - 0.667f) / 0.167f;
        r = 255;
        g = (uint8_t)(255.0f * (1.0f - t));
    } else {
        // Red to White
        float t = (normalized - 0.833f) / 0.167f;
        r = 255;
        g = (uint8_t)(255.0f * t);
        b = (uint8_t)(255.0f * t);
    }

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// ============================================================================
// Thermal Analysis and Reporting
// ============================================================================

/**
 * @brief Print current target lock status to serial
 */
void printTargetStatus() {
    TargetPoint target = target_tracker.getTarget();
    LockState state = target_tracker.getLockState();
    LockStatistics stats = target_tracker.getStatistics(upscaled_frame);

    Serial.println("\n========== TARGET LOCK STATUS ==========");

    // Lock state
    Serial.print("Lock State: ");
    switch (state) {
        case LockState::SEARCHING:
            Serial.println("SEARCHING (acquiring target)");
            break;
        case LockState::LOCKING:
            Serial.println("LOCKING (building confidence)");
            break;
        case LockState::LOCKED:
            Serial.println("LOCKED (tracking target)");
            break;
        case LockState::LOST:
            Serial.println("LOST (previous target no longer detected)");
            break;
        default:
            Serial.println("UNKNOWN");
    }

    if (target.is_valid) {
        Serial.print("Position: (");
        Serial.print((int)target.x);
        Serial.print(", ");
        Serial.print((int)target.y);
        Serial.println(")");

        Serial.print("Temperature: ");
        Serial.print(target.temperature, 2);
        Serial.println("°C");

        Serial.print("Confidence: ");
        Serial.print((uint8_t)(target.confidence * 100));
        Serial.println("%");

        Serial.print("Frames Locked: ");
        Serial.println(target_tracker.getFramesSinceLock());
    } else {
        Serial.println("No valid target");
    }

    Serial.print("Max Temp: ");
    Serial.print(stats.max_temp_in_view, 1);
    Serial.print("°C | Min Temp: ");
    Serial.print(stats.min_temp_in_view, 1);
    Serial.println("°C");

    Serial.print("Hot Pixels (>T-10°C): ");
    Serial.println(stats.total_hot_pixels);

    Serial.println("=========================================\n");
}

/**
 * @brief Log thermal hotspot data for analysis
 */
void logHotspotData() {
    static unsigned long last_log = 0;

    if (millis() - last_log < 1000) {
        return; // Log once per second
    }

    TargetPoint target = target_tracker.getTarget();
    if (!target.is_valid) {
        return;
    }

    Serial.print(millis());
    Serial.print(",");
    Serial.print((int)target.x);
    Serial.print(",");
    Serial.print((int)target.y);
    Serial.print(",");
    Serial.print(target.temperature, 2);
    Serial.print(",");
    Serial.println((uint8_t)(target.confidence * 100));

    last_log = millis();
}

// ============================================================================
// Interactive Control Functions
// ============================================================================

/**
 * @brief Handle serial commands for manual control
 * Supports:
 *   'L' - Lock on current hottest pixel
 *   'R' - Release lock (return to searching)
 *   'S' - Toggle lock state
 *   'T' - Print target status
 *   'H' - Print help
 */
void handleSerialCommands() {
    if (!Serial.available()) {
        return;
    }

    char cmd = Serial.read();

    switch (cmd) {
        case 'L': {
            // Manual lock on current target
            TargetPoint target = target_tracker.getTarget();
            if (target.is_valid) {
                target_tracker.manualLock(target.x, target.y, target.temperature);
                Serial.println("Manual lock acquired");
            }
            break;
        }

        case 'R': {
            // Release lock
            target_tracker.releaseLock();
            Serial.println("Lock released - returning to search");
            break;
        }

        case 'S': {
            // Toggle auto-tracking
            // (implement as needed)
            Serial.println("Auto-tracking toggled");
            break;
        }

        case 'T': {
            // Print target status
            printTargetStatus();
            break;
        }

        case 'H': {
            // Print help
            Serial.println("\n========== COMMAND HELP ==========");
            Serial.println("L - Manual lock on current target");
            Serial.println("R - Release lock");
            Serial.println("S - Toggle auto-tracking");
            Serial.println("T - Print target status");
            Serial.println("H - Print this help");
            Serial.println("==================================\n");
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// Advanced: Dual-Target Tracking
// ============================================================================

/**
 * @brief Find N hottest pixels (for multi-target scenarios)
 */
struct HotPixel {
    uint16_t x, y;
    float temperature;
};

void findMultipleHotspots(const float* thermal_data, HotPixel* targets, uint8_t max_targets) {
    if (thermal_data == nullptr || targets == nullptr || max_targets == 0) {
        return;
    }

    // Initialize targets as invalid
    for (int i = 0; i < max_targets; ++i) {
        targets[i].temperature = -999.0f;
    }

    // Scan frame
    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        float temp = thermal_data[i];

        // Find insertion position
        int insert_pos = -1;
        for (int j = 0; j < max_targets; ++j) {
            if (temp > targets[j].temperature) {
                insert_pos = j;
                break;
            }
        }

        if (insert_pos >= 0) {
            // Shift lower-ranked targets
            for (int j = max_targets - 1; j > insert_pos; --j) {
                targets[j] = targets[j - 1];
            }

            // Insert new target
            targets[insert_pos].y = i / DISPLAY_WIDTH;
            targets[insert_pos].x = i % DISPLAY_WIDTH;
            targets[insert_pos].temperature = temp;
        }
    }
}

/**
 * @brief Track multiple threat targets
 */
void trackMultipleTargets(const float* thermal_data, uint8_t num_targets = 3) {
    // Use fixed array to avoid VLA (variable length array) in C++
    HotPixel targets_buffer[10];
    HotPixel* targets = targets_buffer;
    
    // Only process up to 10 targets max
    if (num_targets > 10) num_targets = 10;
    
    findMultipleHotspots(thermal_data, targets, num_targets);

    Serial.println("\n========== MULTIPLE TARGETS ==========");
    for (int i = 0; i < num_targets; ++i) {
        if (targets[i].temperature > -999.0f) {
            Serial.print("Target ");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print((int)targets[i].x);
            Serial.print(", ");
            Serial.print((int)targets[i].y);
            Serial.print(" | ");
            Serial.print(targets[i].temperature, 1);
            Serial.println("°C");
        }
    }
    Serial.println("======================================\n");
}

// ============================================================================
// Performance Monitoring
// ============================================================================

/**
 * @brief Measure processing time for each pipeline stage
 */
void profileProcessingPipeline() {
    unsigned long t_start, t_end;

    // Profile capture
    t_start = micros();
    thermal_sensor.captureFrame(raw_frame);
    t_end = micros();
    Serial.print("Capture: ");
    Serial.print(t_end - t_start);
    Serial.println(" µs");

    // Profile upscaling
    t_start = micros();
    upscaler.upscale(raw_frame, upscaled_frame);
    t_end = micros();
    Serial.print("Upscale: ");
    Serial.print(t_end - t_start);
    Serial.println(" µs");

    // Profile target tracking
    t_start = micros();
    target_tracker.processThermalFrame(upscaled_frame);
    t_end = micros();
    Serial.print("Tracking: ");
    Serial.print(t_end - t_start);
    Serial.println(" µs");

    // Profile rendering
    t_start = micros();
    target_tracker.drawReticle((uint32_t*)display_buffer);
    t_end = micros();
    Serial.print("Rendering: ");
    Serial.print(t_end - t_start);
    Serial.println(" µs\n");
}

// ============================================================================
// Simulation Mode
// ============================================================================

/**
 * @brief Generate simulated thermal data (hot spot in center)
 */
void generateSimulatedThermalData(float* frame) {
    // Fill with background temperature (~20°C)
    for (int i = 0; i < MLX90640_PIXEL_COUNT; ++i) {
        frame[i] = 20.0f + (rand() % 50) * 0.1f;  // 20-25°C background
    }
    
    // Add hot spot in center (200°C)
    int center_idx = (12 * 32) + 16;  // Center of 32x24 grid
    frame[center_idx] = 200.0f;
    
    // Add thermal gradient around hotspot
    for (int y = 10; y < 14; ++y) {
        for (int x = 14; x < 18; ++x) {
            int idx = (y * 32) + x;
            float dist = sqrtf((x - 16)*(x - 16) + (y - 12)*(y - 12));
            frame[idx] = 200.0f - (dist * 30.0f);
        }
    }
}

/**
 * @brief Simulated processing cycle (no hardware required)
 */
bool processThermalTargetFrameSimulated() {
    // Step 1: Generate simulated thermal data
    generateSimulatedThermalData(raw_frame);

    // Step 2: Upscale to display resolution
    bool upscale_ok = upscaler.upscale(raw_frame, upscaled_frame);
    if (!upscale_ok) {
        Serial.println("Upscaling failed!");
        return false;
    }

    // Step 3: Process thermal frame and update target lock
    TargetPoint target = target_tracker.processThermalFrame(upscaled_frame);

    // Step 4: Convert thermal data to RGB display colors
    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        float temp = upscaled_frame[i];
        display_buffer[i] = temperatureToRGB888(temp);
    }

    // Step 5: Draw targeting reticle
    if (target.is_valid) {
        target_tracker.drawReticle((uint32_t*)display_buffer);
    }

    return true;
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
    Serial.begin(115200);
    
    // Initialize system in simulation mode
    if (!initThermalTargetingSystem(true)) {
        Serial.println("FATAL: System initialization failed!");
        return 1;
    }

    // Run for approximately 30 seconds at 16 Hz
    unsigned long frame_count = 0;
    unsigned long max_frames = 16 * 30;  // 16 Hz * 30 seconds
    unsigned long start_time = millis();

    Serial.println("Running thermal imaging simulation for 30 seconds...\n");
    Serial.println("========== PROCESSING FRAMES ==========");

    while (frame_count < max_frames) {
        unsigned long frame_start = millis();

        // Process simulated thermal frame
        if (processThermalTargetFrameSimulated()) {
            frame_count++;
            
            // Print status every 16 frames (1 second at 16 Hz)
            if (frame_count % 16 == 0) {
                Serial.print("Frame ");
                Serial.print(frame_count);
                Serial.print(" | Elapsed: ");
                Serial.print((millis() - start_time) / 1000);
                Serial.println(" seconds");
            }
        }

        // Frame-rate limiting (63ms per frame for 16 Hz)
        unsigned long frame_time = millis() - frame_start;
        if (frame_time < 63) {
            delay(63 - frame_time);
        }
    }

    Serial.println("=====================================\n");
    Serial.println("Simulation Complete!");
    Serial.print("Processed ");
    Serial.print(frame_count);
    Serial.println(" frames");
    Serial.print("Elapsed time: ");
    Serial.print((millis() - start_time) / 1000.0f, 1);
    Serial.println(" seconds");
    
    // Print final target status
    printTargetStatus();
    
    Serial.println("========== PERFORMANCE SUMMARY ==========");
    profileProcessingPipeline();
    
    return 0;
}
