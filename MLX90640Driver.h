#ifndef MLX90640_DRIVER_H
#define MLX90640_DRIVER_H

#include <cstdint>
#include <cstring>

// I2C Configuration
#define MLX90640_I2C_SPEED_HZ 400000
#define MLX90640_SLAVE_ADDR 0x33

// Sensor Parameters
#define MLX90640_WIDTH 32
#define MLX90640_HEIGHT 24
#define MLX90640_PIXEL_COUNT (MLX90640_WIDTH * MLX90640_HEIGHT)
#define MLX90640_RAM_SIZE 832

// MLX90640 I2C Register Addresses
#define MLX90640_REG_CTL1 0x800D
#define MLX90640_REG_CTL2 0x800E
#define MLX90640_REG_STAT 0x8000
#define MLX90640_REG_FRAME 0x8005
#define MLX90640_REG_RAM_BASE 0x0400

// Control Register 1 (0x800D) - Refresh Rate bits
#define MLX90640_REFRESH_RATE_MASK 0x0F00
#define MLX90640_REFRESH_RATE_SHIFT 8
#define MLX90640_REFRESH_RATE_16HZ 0x04

// Status Register bits
#define MLX90640_DATA_READY_BIT 0x0008

/**
 * @enum MLX90640_Status
 * @brief Return status codes for MLX90640 operations
 */
enum class MLX90640_Status : uint8_t {
    OK = 0,                    // Operation successful
    SENSOR_NOT_FOUND = 1,      // Sensor not detected on I2C bus
    I2C_ERROR = 2,             // I2C communication error
    CHECKSUM_ERROR = 3,        // Frame data checksum invalid
    DATA_NOT_READY = 4,        // Sensor data not ready
    INVALID_PARAMETER = 5,     // Invalid parameter provided
    TIMEOUT = 6                // Operation timeout
};

/**
 * @class MLX90640Driver
 * @brief Driver class for MLX90640 thermal imaging sensor
 *
 * Provides I2C interface for reading thermal data from MLX90640 sensor.
 * Designed for ESP32 with minimal heap allocation using static arrays.
 */
class MLX90640Driver {
public:
    MLX90640Driver();
    ~MLX90640Driver() = default;

    /**
     * @brief Initialize I2C interface and verify sensor presence
     * @param sda_pin GPIO pin for I2C SDA line
     * @param scl_pin GPIO pin for I2C SCL line
     * @return MLX90640_Status: OK if successful, SENSOR_NOT_FOUND if not detected
     */
    MLX90640_Status init(uint8_t sda_pin, uint8_t scl_pin);

    /**
     * @brief Capture thermal frame and convert to float array
     * @param frame_data Output: pointer to float array of size MLX90640_PIXEL_COUNT
     * @return MLX90640_Status: OK if successful
     */
    MLX90640_Status captureFrame(float* frame_data);

    /**
     * @brief Set sensor refresh rate
     * @param refresh_rate Refresh rate in Hz (0.5, 1, 2, 4, 8, 16 Hz)
     * @return MLX90640_Status: OK if successful, INVALID_PARAMETER if rate not supported
     */
    MLX90640_Status setRefreshRate(float refresh_rate);

    /**
     * @brief Set refresh rate to 16Hz
     * @return MLX90640_Status: OK if successful
     */
    MLX90640_Status setRefreshRate16Hz();

    /**
     * @brief Get initialization status
     * @return true if sensor is initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get last error status
     * @return MLX90640_Status from last operation
     */
    MLX90640_Status getLastError() const { return last_error_; }

private:
    // State variables
    bool initialized_;
    MLX90640_Status last_error_;
    uint8_t i2c_addr_;

    // Static buffers to minimize heap allocation
    uint16_t eeprom_data_[256];     // EEPROM calibration data
    uint16_t frame_data_[MLX90640_RAM_SIZE];  // Raw frame data buffer

    /**
     * @brief Read 16-bit register from sensor
     * @param reg_addr Register address
     * @param value Output: register value
     * @return MLX90640_Status: OK if successful
     */
    MLX90640_Status readRegister(uint16_t reg_addr, uint16_t* value);

    /**
     * @brief Write 16-bit register to sensor
     * @param reg_addr Register address
     * @param value Value to write
     * @return MLX90640_Status: OK if successful
     */
    MLX90640_Status writeRegister(uint16_t reg_addr, uint16_t value);

    /**
     * @brief Read multiple 16-bit registers
     * @param start_addr Starting register address
     * @param count Number of registers to read
     * @param buffer Output: buffer to store values
     * @return MLX90640_Status: OK if successful
     */
    MLX90640_Status readRegisters(uint16_t start_addr, uint16_t count, uint16_t* buffer);

    /**
     * @brief Read raw frame data from sensor RAM
     * @return MLX90640_Status: OK if successful
     */
    MLX90640_Status readRawFrame();

    /**
     * @brief Verify frame data checksum
     * @return true if checksum is valid
     */
    bool verifyFrameChecksum();

    /**
     * @brief Check if new frame data is available
     * @return true if data ready flag is set
     */
    bool isDataReady();

    /**
     * @brief Check I2C bus and sensor presence
     * @return true if sensor responds on I2C bus
     */
    bool detectSensor();

    /**
     * @brief Load calibration data from sensor EEPROM
     * @return MLX90640_Status: OK if successful
     */
    MLX90640_Status loadCalibrationData();

    /**
     * @brief Convert raw sensor data to temperature values
     * @param frame_data Output: float array for temperature values
     */
    void processRawFrame(float* frame_data);
};

#endif // MLX90640_DRIVER_H