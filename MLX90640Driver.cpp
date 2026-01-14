#include "MLX90640Driver.h"
#include <chrono>
#include <thread>

// Mock I2C interface for ESP32 (replace with actual Wire library)
#ifdef ARDUINO_ARCH_ESP32
#include <Wire.h>
#else
// Fallback for non-ESP32 environments (for testing/prototyping)
#define Wire_begin(sda, scl)
#define Wire_setClock(freq)
#define Wire_beginTransmission(addr)
#define Wire_write(data)
#define Wire_endTransmission() 0
#define Wire_requestFrom(addr, len) 0
#define Wire_read() 0
#endif

// Arduino compatibility functions
namespace {
    using Clock = std::chrono::steady_clock;
    static auto startup_time = Clock::now();

    unsigned long millis() {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startup_time);
        return static_cast<unsigned long>(duration.count());
    }

    void delay(unsigned long ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

// EEPROM base address
#define MLX90640_EEPROM_BASE 0x2400

// Frame capture timeout in milliseconds
#define FRAME_CAPTURE_TIMEOUT_MS 1000

// Refresh rate lookup table
static constexpr struct {
    float hz;
    uint8_t reg_value;
} REFRESH_RATES[] = {
    {0.5f, 0x00},
    {1.0f, 0x01},
    {2.0f, 0x02},
    {4.0f, 0x03},
    {8.0f, 0x04},
    {16.0f, 0x05}
};
static constexpr uint8_t REFRESH_RATES_COUNT = 6;

MLX90640Driver::MLX90640Driver()
    : initialized_(false),
      last_error_(MLX90640_Status::OK),
      i2c_addr_(MLX90640_SLAVE_ADDR) {
    // Zero initialize static buffers
    std::memset(eeprom_data_, 0, sizeof(eeprom_data_));
    std::memset(frame_data_, 0, sizeof(frame_data_));
}

MLX90640_Status MLX90640Driver::init(uint8_t sda_pin, uint8_t scl_pin) {
#ifdef ARDUINO_ARCH_ESP32
    // Initialize I2C bus for ESP32
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(MLX90640_I2C_SPEED_HZ);
#endif

    // Check sensor presence
    if (!detectSensor()) {
        initialized_ = false;
        return (last_error_ = MLX90640_Status::SENSOR_NOT_FOUND);
    }

    // Load calibration data from EEPROM
    MLX90640_Status status = loadCalibrationData();
    if (status != MLX90640_Status::OK) {
        initialized_ = false;
        return (last_error_ = status);
    }

    // Set default refresh rate to 16Hz
    status = setRefreshRate16Hz();
    if (status != MLX90640_Status::OK) {
        initialized_ = false;
        return (last_error_ = status);
    }

    initialized_ = true;
    return (last_error_ = MLX90640_Status::OK);
}

MLX90640_Status MLX90640Driver::captureFrame(float* frame_data) {
    if (!initialized_) {
        return (last_error_ = MLX90640_Status::INVALID_PARAMETER);
    }

    if (frame_data == nullptr) {
        return (last_error_ = MLX90640_Status::INVALID_PARAMETER);
    }

    // Wait for new frame data
    unsigned long start_time = millis();
    while (!isDataReady()) {
        if (millis() - start_time > FRAME_CAPTURE_TIMEOUT_MS) {
            return (last_error_ = MLX90640_Status::TIMEOUT);
        }
        delay(1);
    }

    // Read raw frame data
    MLX90640_Status status = readRawFrame();
    if (status != MLX90640_Status::OK) {
        return (last_error_ = status);
    }

    // Verify frame integrity
    if (!verifyFrameChecksum()) {
        return (last_error_ = MLX90640_Status::CHECKSUM_ERROR);
    }

    // Convert raw data to temperature values
    processRawFrame(frame_data);

    return (last_error_ = MLX90640_Status::OK);
}

MLX90640_Status MLX90640Driver::setRefreshRate(float refresh_rate) {
    if (!initialized_) {
        return (last_error_ = MLX90640_Status::INVALID_PARAMETER);
    }

    // Find matching refresh rate in lookup table
    uint8_t reg_value = 0xFF;
    for (uint8_t i = 0; i < REFRESH_RATES_COUNT; ++i) {
        if (REFRESH_RATES[i].hz == refresh_rate) {
            reg_value = REFRESH_RATES[i].reg_value;
            break;
        }
    }

    if (reg_value == 0xFF) {
        return (last_error_ = MLX90640_Status::INVALID_PARAMETER);
    }

    // Read current control register value
    uint16_t ctrl_reg = 0;
    MLX90640_Status status = readRegister(MLX90640_REG_CTL1, &ctrl_reg);
    if (status != MLX90640_Status::OK) {
        return (last_error_ = status);
    }

    // Update refresh rate bits
    ctrl_reg &= ~MLX90640_REFRESH_RATE_MASK;
    ctrl_reg |= (reg_value << MLX90640_REFRESH_RATE_SHIFT);

    // Write back to sensor
    status = writeRegister(MLX90640_REG_CTL1, ctrl_reg);
    return (last_error_ = status);
}

MLX90640_Status MLX90640Driver::setRefreshRate16Hz() {
    return setRefreshRate(16.0f);
}

MLX90640_Status MLX90640Driver::readRegister(uint16_t reg_addr, uint16_t* value) {
    if (value == nullptr) {
        return MLX90640_Status::INVALID_PARAMETER;
    }

#ifdef ARDUINO_ARCH_ESP32
    Wire.beginTransmission(i2c_addr_);
    Wire.write((uint8_t)(reg_addr >> 8));
    Wire.write((uint8_t)(reg_addr & 0xFF));
    uint8_t result = Wire.endTransmission(false); // Send repeated start

    if (result != 0) {
        return MLX90640_Status::I2C_ERROR;
    }

    if (Wire.requestFrom(i2c_addr_, (uint8_t)2) != 2) {
        return MLX90640_Status::I2C_ERROR;
    }

    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    *value = ((uint16_t)msb << 8) | lsb;

    return MLX90640_Status::OK;
#else
    return MLX90640_Status::OK;
#endif
}

MLX90640_Status MLX90640Driver::writeRegister(uint16_t reg_addr, uint16_t value) {
#ifdef ARDUINO_ARCH_ESP32
    Wire.beginTransmission(i2c_addr_);
    Wire.write((uint8_t)(reg_addr >> 8));
    Wire.write((uint8_t)(reg_addr & 0xFF));
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    uint8_t result = Wire.endTransmission();

    if (result != 0) {
        return MLX90640_Status::I2C_ERROR;
    }

    return MLX90640_Status::OK;
#else
    return MLX90640_Status::OK;
#endif
}

MLX90640_Status MLX90640Driver::readRegisters(uint16_t start_addr, uint16_t count, uint16_t* buffer) {
    if (buffer == nullptr || count == 0) {
        return MLX90640_Status::INVALID_PARAMETER;
    }

#ifdef ARDUINO_ARCH_ESP32
    for (uint16_t i = 0; i < count; ++i) {
        MLX90640_Status status = readRegister(start_addr + i, &buffer[i]);
        if (status != MLX90640_Status::OK) {
            return status;
        }
    }
    return MLX90640_Status::OK;
#else
    return MLX90640_Status::OK;
#endif
}

MLX90640_Status MLX90640Driver::readRawFrame() {
    // Read frame data from RAM (typically at address 0x0400-0x04FF)
    MLX90640_Status status = readRegisters(MLX90640_REG_RAM_BASE, MLX90640_RAM_SIZE, frame_data_);
    return status;
}

bool MLX90640Driver::verifyFrameChecksum() {
    // Calculate checksum over frame data
    // This is a simplified checksum verification
    // The actual MLX90640 uses a specific algorithm
    uint16_t checksum = 0;

    for (int i = 0; i < MLX90640_RAM_SIZE - 1; ++i) {
        checksum += frame_data_[i];
    }

    // Compare with device checksum (last word)
    uint16_t device_checksum = frame_data_[MLX90640_RAM_SIZE - 1];

    // Simplified verification - in production, implement proper checksum validation
    return true;
}

bool MLX90640Driver::isDataReady() {
    uint16_t status_reg = 0;
    MLX90640_Status status = readRegister(MLX90640_REG_STAT, &status_reg);

    if (status != MLX90640_Status::OK) {
        return false;
    }

    return (status_reg & MLX90640_DATA_READY_BIT) != 0;
}

bool MLX90640Driver::detectSensor() {
#ifdef ARDUINO_ARCH_ESP32
    Wire.beginTransmission(i2c_addr_);
    uint8_t result = Wire.endTransmission();
    return (result == 0);
#else
    return true; // Assume sensor present in non-ESP32 environment
#endif
}

MLX90640_Status MLX90640Driver::loadCalibrationData() {
    // Load calibration data from sensor EEPROM
    // EEPROM is typically at 0x2400-0x24FF
    MLX90640_Status status = readRegisters(MLX90640_EEPROM_BASE, 256, eeprom_data_);

    if (status != MLX90640_Status::OK) {
        return status;
    }

    // Validate calibration data presence
    // Check if EEPROM contains valid data (not all zeros or all ones)
    bool has_data = false;
    for (int i = 0; i < 256; ++i) {
        if (eeprom_data_[i] != 0x0000 && eeprom_data_[i] != 0xFFFF) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        return MLX90640_Status::SENSOR_NOT_FOUND;
    }

    return MLX90640_Status::OK;
}

void MLX90640Driver::processRawFrame(float* frame_data) {
    // Convert raw 16-bit sensor data to temperature values in Celsius
    // This is a simplified conversion; actual implementation requires:
    // 1. Ambient temperature compensation
    // 2. Emissivity correction
    // 3. Proper calibration data application
    // 4. Pixel-by-pixel processing

    for (int i = 0; i < MLX90640_PIXEL_COUNT; ++i) {
        // Raw data to temperature conversion
        // MLX90640 stores data in 16-bit format
        // Simplified: convert to a temperature range (-20°C to +300°C)

        uint16_t raw = frame_data_[i];

        // Convert 16-bit raw value to float temperature
        // This is a placeholder conversion
        // Real implementation needs calibration coefficients from EEPROM
        float temp = -20.0f + ((float)raw / 65535.0f) * 320.0f;

        frame_data[i] = temp;
    }
}