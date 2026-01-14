#include "MLX90640Driver.h"

// Pin definitions for ESP32
#define SDA_PIN 21
#define SCL_PIN 22

// Global driver instance
static MLX90640Driver thermal_sensor;

// Static buffer for frame data (768 pixels)
static float frame_buffer[MLX90640_PIXEL_COUNT];

/**
 * @brief Initialize thermal imaging system
 * @return true if successful, false otherwise
 */
bool initThermalSystem() {
    // Initialize the MLX90640 driver
    MLX90640_Status status = thermal_sensor.init(SDA_PIN, SCL_PIN);

    if (status != MLX90640_Status::OK) {
        // Handle initialization error
        switch (status) {
            case MLX90640_Status::SENSOR_NOT_FOUND:
                Serial.println("ERROR: MLX90640 sensor not found on I2C bus!");
                break;
            case MLX90640_Status::I2C_ERROR:
                Serial.println("ERROR: I2C communication error during initialization!");
                break;
            default:
                Serial.println("ERROR: Unknown error during sensor initialization!");
                break;
        }
        return false;
    }

    Serial.println("MLX90640 thermal sensor initialized successfully");

    // Set refresh rate to 16Hz
    status = thermal_sensor.setRefreshRate16Hz();
    if (status != MLX90640_Status::OK) {
        Serial.println("ERROR: Failed to set refresh rate to 16Hz!");
        return false;
    }

    Serial.println("Refresh rate set to 16Hz");
    return true;
}

/**
 * @brief Capture and process thermal frame
 * @return true if successful, false otherwise
 */
bool captureThermalFrame() {
    if (!thermal_sensor.isInitialized()) {
        Serial.println("ERROR: Thermal sensor not initialized!");
        return false;
    }

    // Capture frame from sensor
    MLX90640_Status status = thermal_sensor.captureFrame(frame_buffer);

    if (status != MLX90640_Status::OK) {
        // Handle capture error
        switch (status) {
            case MLX90640_Status::TIMEOUT:
                Serial.println("ERROR: Timeout waiting for frame data!");
                break;
            case MLX90640_Status::CHECKSUM_ERROR:
                Serial.println("ERROR: Frame data checksum validation failed!");
                break;
            case MLX90640_Status::I2C_ERROR:
                Serial.println("ERROR: I2C communication error during frame capture!");
                break;
            default:
                Serial.println("ERROR: Unknown error during frame capture!");
                break;
        }
        return false;
    }

    return true;
}

/**
 * @brief Print thermal frame data (for debugging)
 */
void printThermalFrame() {
    Serial.println("Thermal Frame Data (32x24 grid):");
    Serial.println("=====================================");

    for (int y = 0; y < MLX90640_HEIGHT; ++y) {
        for (int x = 0; x < MLX90640_WIDTH; ++x) {
            int index = y * MLX90640_WIDTH + x;
            float temp = frame_buffer[index];
            Serial.print(temp, 1);
            Serial.print(" ");
        }
        Serial.println();
    }
    Serial.println("=====================================");
}

/**
 * @brief Get minimum temperature in frame
 * @return Minimum temperature in Celsius
 */
float getMinTemperature() {
    float min_temp = frame_buffer[0];
    for (int i = 1; i < MLX90640_PIXEL_COUNT; ++i) {
        if (frame_buffer[i] < min_temp) {
            min_temp = frame_buffer[i];
        }
    }
    return min_temp;
}

/**
 * @brief Get maximum temperature in frame
 * @return Maximum temperature in Celsius
 */
float getMaxTemperature() {
    float max_temp = frame_buffer[0];
    for (int i = 1; i < MLX90640_PIXEL_COUNT; ++i) {
        if (frame_buffer[i] > max_temp) {
            max_temp = frame_buffer[i];
        }
    }
    return max_temp;
}

/**
 * @brief Get temperature at specific pixel
 * @param x X coordinate (0-31)
 * @param y Y coordinate (0-23)
 * @return Temperature in Celsius, or NaN if out of bounds
 */
float getPixelTemperature(uint8_t x, uint8_t y) {
    if (x >= MLX90640_WIDTH || y >= MLX90640_HEIGHT) {
        return NAN;
    }
    return frame_buffer[y * MLX90640_WIDTH + x];
}

// ============================================================================
// Main Arduino Setup and Loop (for ESP32)
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=================================");
    Serial.println("MLX90640 Thermal Imaging System");
    Serial.println("=================================\n");

    // Initialize thermal imaging system
    if (!initThermalSystem()) {
        Serial.println("Failed to initialize thermal imaging system. Halting.");
        while (1) {
            delay(1000);
        }
    }

    Serial.println("System ready for thermal imaging.\n");
}

void loop() {
    // Capture thermal frame
    if (!captureThermalFrame()) {
        Serial.println("Failed to capture frame. Retrying...\n");
        delay(100);
        return;
    }

    // Get temperature statistics
    float min_temp = getMinTemperature();
    float max_temp = getMaxTemperature();

    // Print results
    Serial.print("Frame captured | Min: ");
    Serial.print(min_temp, 2);
    Serial.print("°C | Max: ");
    Serial.print(max_temp, 2);
    Serial.println("°C");

    // Example: Get temperature at center pixel (16, 12)
    float center_temp = getPixelTemperature(16, 12);
    Serial.print("Center pixel temp: ");
    Serial.print(center_temp, 2);
    Serial.println("°C\n");

    // Optionally print full frame (comment out for performance)
    // printThermalFrame();

    // Process at ~16Hz (sensor refresh rate)
    delay(63); // ~63ms for 16Hz operation
}

// ============================================================================
// Alternative: Single-shot capture example
// ============================================================================

/**
 * @brief Capture single frame and process it
 * This can be called from interrupt handlers or task schedulers
 */
void processSingleFrame() {
    if (captureThermalFrame()) {
        float min_temp = getMinTemperature();
        float max_temp = getMaxTemperature();

        Serial.print("Min: ");
        Serial.print(min_temp, 1);
        Serial.print("°C, Max: ");
        Serial.print(max_temp, 1);
        Serial.println("°C");
    }
}

// ============================================================================
// Error handling and recovery example
// ============================================================================

/**
 * @brief Check sensor health and recover from errors
 */
void checkSensorHealth() {
    if (!thermal_sensor.isInitialized()) {
        Serial.println("Sensor not initialized. Attempting reinitialize...");

        if (!initThermalSystem()) {
            Serial.println("Reinitialization failed.");
            return;
        }
    }

    // Check last error status
    MLX90640_Status last_error = thermal_sensor.getLastError();
    if (last_error != MLX90640_Status::OK) {
        Serial.print("Last error code: ");
        Serial.println((uint8_t)last_error);

        // Implement recovery strategy based on error type
        switch (last_error) {
            case MLX90640_Status::CHECKSUM_ERROR:
                Serial.println("Checksum error detected. Retrying capture...");
                break;
            case MLX90640_Status::TIMEOUT:
                Serial.println("Timeout detected. Restarting sensor...");
                initThermalSystem();
                break;
            default:
                break;
        }
    }
}
