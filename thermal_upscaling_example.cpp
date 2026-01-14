#include "MLX90640Driver.h"
#include "MLX90640Upscaler.h"

// ============================================================================
// Configuration
// ============================================================================

#define SDA_PIN 21
#define SCL_PIN 22

// For 320x240 display (typical TFT on ESP32)
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Static instances (no heap allocation)
static MLX90640Driver thermal_sensor;
static MLX90640Upscaler upscaler;

// Buffers for raw and upscaled data
static float raw_frame[MLX90640_PIXEL_COUNT];        // 768 pixels
static float upscaled_frame[DISPLAY_WIDTH * DISPLAY_HEIGHT]; // 76,800 pixels

// ============================================================================
// Thermal Imaging Display System Setup
// ============================================================================

/**
 * @brief Initialize thermal sensor and display system
 * @return true if successful
 */
bool initThermalDisplaySystem() {
    Serial.println("Initializing thermal imaging display system...");

    // Initialize thermal sensor
    MLX90640_Status status = thermal_sensor.init(SDA_PIN, SCL_PIN);
    if (status != MLX90640_Status::OK) {
        Serial.println("ERROR: Failed to initialize MLX90640 sensor!");
        return false;
    }

    // Set 16 Hz refresh rate
    status = thermal_sensor.setRefreshRate16Hz();
    if (status != MLX90640_Status::OK) {
        Serial.println("ERROR: Failed to set refresh rate!");
        return false;
    }

    // Initialize upscaler (bilinear interpolation)
    upscaler.setInterpolationMethod(InterpolationMethod::BILINEAR);

    Serial.println("Thermal display system initialized");
    Serial.print("  Sensor: 32x24 (768 pixels)");
    Serial.print("  Display: 320x240 (76,800 pixels)");
    Serial.println("  Upscale factor: 10x");

    return true;
}

// ============================================================================
// Core Thermal Imaging Loop
// ============================================================================

/**
 * @brief Capture and upscale thermal frame
 * @return true if successful
 */
bool captureAndUpscaleThermalFrame() {
    // Capture raw 32x24 frame
    MLX90640_Status status = thermal_sensor.captureFrame(raw_frame);
    if (status != MLX90640_Status::OK) {
        Serial.print("ERROR: Frame capture failed (code ");
        Serial.print((uint8_t)status);
        Serial.println(")");
        return false;
    }

    // Upscale to 320x240 using bilinear interpolation
    if (!upscaler.upscale(raw_frame, upscaled_frame)) {
        Serial.println("ERROR: Upscaling failed!");
        return false;
    }

    return true;
}

/**
 * @brief Capture, upscale, and apply contrast stretching
 * @return true if successful
 */
bool captureAndDisplayThermalImage() {
    if (!captureAndUpscaleThermalFrame()) {
        return false;
    }

    // Optional: Apply contrast stretching for better visibility
    // This normalizes data to 0-100 range for display
    MLX90640Upscaler::stretchContrast(upscaled_frame, DISPLAY_WIDTH * DISPLAY_HEIGHT);

    return true;
}

// ============================================================================
// Display Output Functions (Platform-Specific)
// ============================================================================

/**
 * @brief Send upscaled thermal image to 320x240 TFT display
 *
 * This is a template for integration with various display drivers:
 * - ILI9341 (common 320x240 TFT)
 * - ST7789 (common 240x320 TFT)
 * - Custom displays via SPI/I2C
 */
void displayThermalImage() {
    // Example: Assume a display library with setPixelColor(x, y, color)
    // This is pseudocode - adapt to your specific display driver

    for (uint16_t y = 0; y < DISPLAY_HEIGHT; ++y) {
        for (uint16_t x = 0; x < DISPLAY_WIDTH; ++x) {
            uint32_t pixel_index = y * DISPLAY_WIDTH + x;
            float temperature = upscaled_frame[pixel_index];

            // Convert temperature to color
            // Temperature range: -20°C to +300°C (or 0-100 if contrast-stretched)
            uint32_t color = temperatureToColor(temperature);

            // Update display pixel
            // display.drawPixel(x, y, color);
        }
    }

    // Update entire display at once
    // display.updateDisplay();
}

/**
 * @brief Optimized row-by-row thermal display update
 *
 * For bandwidth-limited displays, update one row at a time
 * Useful for LCDs connected via SPI with limited bandwidth
 */
void displayThermalImageRowByRow() {
    float output_row[DISPLAY_WIDTH];

    for (uint16_t row = 0; row < DISPLAY_HEIGHT; ++row) {
        // Upscale single output row from thermal data
        if (!upscaler.upscaleRow(raw_frame, output_row, row)) {
            Serial.println("ERROR: Row upscaling failed!");
            return;
        }

        // Send row to display
        for (uint16_t col = 0; col < DISPLAY_WIDTH; ++col) {
            float temperature = output_row[col];
            uint32_t color = temperatureToColor(temperature);
            // display.drawPixel(col, row, color);
        }
    }
}

/**
 * @brief Convert temperature to RGB color for thermal display
 *
 * Implements pseudo-color mapping:
 * Black (cold) -> Blue -> Cyan -> Green -> Yellow -> Red -> White (hot)
 *
 * @param temperature Value in °C (or 0-100 if normalized)
 * @return 24-bit RGB color (0xRRGGBB)
 */
uint32_t temperatureToColor(float temperature) {
    // Normalize to 0-1 range (assuming -20°C to +300°C range)
    // Adjust these values based on your expected temperature range
    float min_temp = -20.0f;
    float max_temp = 300.0f;
    float normalized = (temperature - min_temp) / (max_temp - min_temp);

    // Clamp to 0-1
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    // Pseudo-color mapping (6 segments)
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
// Thermal Analysis Functions
// ============================================================================

/**
 * @brief Find hottest region in thermal image
 * @param hotspot_x Output: X coordinate of hottest pixel
 * @param hotspot_y Output: Y coordinate of hottest pixel
 * @return Maximum temperature value
 */
float findHotspot(uint16_t& hotspot_x, uint16_t& hotspot_y) {
    float max_temp = upscaled_frame[0];
    uint32_t max_index = 0;

    for (uint32_t i = 1; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        if (upscaled_frame[i] > max_temp) {
            max_temp = upscaled_frame[i];
            max_index = i;
        }
    }

    // Convert linear index to X, Y
    hotspot_y = max_index / DISPLAY_WIDTH;
    hotspot_x = max_index % DISPLAY_WIDTH;

    return max_temp;
}

/**
 * @brief Calculate average temperature in region
 * @param x1, y1, x2, y2 Region bounds (inclusive)
 * @return Average temperature
 */
float getRegionAverageTemp(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    if (x1 >= DISPLAY_WIDTH || y1 >= DISPLAY_HEIGHT ||
        x2 >= DISPLAY_WIDTH || y2 >= DISPLAY_HEIGHT) {
        return 0.0f;
    }

    float sum = 0.0f;
    uint32_t count = 0;

    for (uint16_t y = y1; y <= y2; ++y) {
        for (uint16_t x = x1; x <= x2; ++x) {
            sum += upscaled_frame[y * DISPLAY_WIDTH + x];
            count++;
        }
    }

    return count > 0 ? sum / count : 0.0f;
}

/**
 * @brief Get interpolated temperature at specific display pixel
 * @param x X coordinate
 * @param y Y coordinate
 * @return Temperature at that pixel
 */
float getDisplayPixelTemperature(uint16_t x, uint16_t y) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        return 0.0f;
    }
    return upscaled_frame[y * DISPLAY_WIDTH + x];
}

// ============================================================================
// Debug and Diagnostic Functions
// ============================================================================

/**
 * @brief Print raw 32x24 frame (for debugging)
 */
void printRawFrame() {
    Serial.println("\n=== RAW THERMAL FRAME (32x24) ===");
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 32; ++x) {
            Serial.print(raw_frame[y * 32 + x], 1);
            Serial.print(" ");
        }
        Serial.println();
    }
}

/**
 * @brief Print thermal statistics
 */
void printThermalStats() {
    float min_temp = raw_frame[0];
    float max_temp = raw_frame[0];
    float sum = 0.0f;

    for (int i = 0; i < MLX90640_PIXEL_COUNT; ++i) {
        if (raw_frame[i] < min_temp) min_temp = raw_frame[i];
        if (raw_frame[i] > max_temp) max_temp = raw_frame[i];
        sum += raw_frame[i];
    }

    float avg_temp = sum / MLX90640_PIXEL_COUNT;

    Serial.println("\n=== THERMAL STATISTICS ===");
    Serial.print("Min: ");
    Serial.print(min_temp, 2);
    Serial.println("°C");
    Serial.print("Max: ");
    Serial.print(max_temp, 2);
    Serial.println("°C");
    Serial.print("Avg: ");
    Serial.print(avg_temp, 2);
    Serial.println("°C");
}

/**
 * @brief Profile upscaling performance
 */
void profileUpscaling() {
    unsigned long start_time = millis();

    for (int i = 0; i < 10; ++i) {
        upscaler.upscale(raw_frame, upscaled_frame);
    }

    unsigned long elapsed = millis() - start_time;
    float avg_time_ms = (float)elapsed / 10.0f;

    Serial.println("\n=== UPSCALING PERFORMANCE ===");
    Serial.print("10 upscales in ");
    Serial.print(elapsed);
    Serial.println(" ms");
    Serial.print("Average: ");
    Serial.print(avg_time_ms, 1);
    Serial.println(" ms per frame");
    Serial.print("FPS: ");
    Serial.println(1000.0f / avg_time_ms, 1);
}

// ============================================================================
// Main Arduino Sketch (Setup and Loop)
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=====================================");
    Serial.println("MLX90640 Thermal Imaging - Upscaler");
    Serial.println("=====================================\n");

    // Initialize thermal imaging system
    if (!initThermalDisplaySystem()) {
        Serial.println("FATAL: System initialization failed!");
        while (1) delay(1000);
    }

    Serial.println("\nSystem ready for thermal imaging.\n");

    // Optional: Profile performance on startup
    // profileUpscaling();
}

void loop() {
    // Capture and upscale thermal frame
    if (!captureAndDisplayThermalImage()) {
        Serial.println("Frame capture failed. Retrying...");
        delay(100);
        return;
    }

    // Print statistics for debugging
    static unsigned long last_stats = 0;
    if (millis() - last_stats > 5000) {
        printThermalStats();
        last_stats = millis();
    }

    // Send to display (implement display update here)
    // displayThermalImage();
    // or displayThermalImageRowByRow();

    // Process at ~16 Hz (sensor refresh rate)
    // This ensures we don't miss any frames
    delay(63); // ~63ms for 16 Hz
}

// ============================================================================
// Alternative: Interrupt-driven thermal imaging with DMA
// ============================================================================

/**
 * @brief High-performance thermal update for interrupt handlers
 *
 * Call from timer interrupt to maintain consistent 16 Hz frame rate
 * while leaving CPU time for other tasks
 */
void ISR_ThermalFrameUpdate() {
    if (!captureAndUpscaleThermalFrame()) {
        return; // Skip this frame on error
    }

    // Apply any post-processing
    // MLX90640Upscaler::stretchContrast(upscaled_frame, DISPLAY_WIDTH * DISPLAY_HEIGHT);

    // Signal display update (non-blocking)
    // triggerDisplayUpdate();
}

// ============================================================================
// Optional: Streaming thermal data over serial/network
// ============================================================================

/**
 * @brief Stream upscaled thermal data in CSV format for visualization
 * @param max_rows Maximum rows to stream (for bandwidth limiting)
 */
void streamThermalDataCSV(uint16_t max_rows = 240) {
    Serial.println("x,y,temperature");

    for (uint16_t y = 0; y < max_rows; ++y) {
        for (uint16_t x = 0; x < DISPLAY_WIDTH; ++x) {
            Serial.print(x);
            Serial.print(",");
            Serial.print(y);
            Serial.print(",");
            Serial.println(upscaled_frame[y * DISPLAY_WIDTH + x], 2);
        }
    }
}

/**
 * @brief Stream as binary data for efficient transmission
 */
void streamThermalDataBinary() {
    // Header: THERM\n32x24->320x240\n
    Serial.write((uint8_t*)"THERM", 5);

    // Stream as 16-bit fixed-point values
    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        int16_t temp_fixed = (int16_t)(upscaled_frame[i] * 100);
        Serial.write((uint8_t)(temp_fixed >> 8));
        Serial.write((uint8_t)(temp_fixed & 0xFF));
    }
}
