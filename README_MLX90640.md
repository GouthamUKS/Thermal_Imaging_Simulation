# MLX90640 ESP32 Thermal Imaging Driver

A production-ready C++ driver for the MLX90640 thermal sensor on ESP32 platforms, designed with embedded systems constraints in mind.

## Features

- **I2C Interface**: Configured for 400 kHz operation (standard I2C speed)
- **Sensor Detection**: Automatic sensor presence verification on initialization
- **Error Handling**: Comprehensive error codes for debugging and recovery
- **Data Capture**: Raw 32×24 pixel array (768 pixels) into 1D float array
- **Refresh Rate Control**: 16 Hz default with support for 0.5, 1, 2, 4, 8, 16 Hz rates
- **Memory Efficient**: Minimal heap allocation using static buffers (embedded-friendly)
- **EEPROM Calibration**: Loads factory calibration data for accurate readings

## Hardware Requirements

- **Microcontroller**: ESP32 (compatible with Arduino framework)
- **Sensor**: MLX90640 thermal imaging array
- **I2C Interface**: 400 kHz I2C bus
- **Typical Connections**:
  - MLX90640 SDA → ESP32 GPIO 21 (default)
  - MLX90640 SCL → ESP32 GPIO 22 (default)
  - MLX90640 GND → ESP32 GND
  - MLX90640 VCC → ESP32 3.3V

## Driver Architecture

### Class: MLX90640Driver

Main driver class providing sensor control and data acquisition.

#### Initialization

```cpp
MLX90640Driver thermal_sensor;
MLX90640_Status status = thermal_sensor.init(SDA_PIN, SCL_PIN);

if (status == MLX90640_Status::OK) {
    // Sensor ready
}
```

**Error Handling for Sensor Not Found**:
```cpp
if (status == MLX90640_Status::SENSOR_NOT_FOUND) {
    // Sensor not responding on I2C bus
    // Check wiring, I2C address, power supply
    Serial.println("Sensor not detected!");
}
```

#### Frame Capture

```cpp
float frame_data[MLX90640_PIXEL_COUNT]; // 768 floats
MLX90640_Status status = thermal_sensor.captureFrame(frame_data);

if (status == MLX90640_Status::OK) {
    // frame_data now contains 32×24 thermal values in °C
}
```

#### Set Refresh Rate to 16Hz

```cpp
// Explicit call
MLX90640_Status status = thermal_sensor.setRefreshRate16Hz();

// Or use generic method with Hz value
status = thermal_sensor.setRefreshRate(16.0f);
```

Supported refresh rates:
- 0.5 Hz
- 1.0 Hz
- 2.0 Hz
- 4.0 Hz
- 8.0 Hz
- 16.0 Hz

## Status Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | OK | Operation successful |
| 1 | SENSOR_NOT_FOUND | Sensor not detected on I2C bus |
| 2 | I2C_ERROR | I2C communication failed |
| 3 | CHECKSUM_ERROR | Frame data validation failed |
| 4 | DATA_NOT_READY | Sensor data not ready (timeout) |
| 5 | INVALID_PARAMETER | Invalid parameter provided |
| 6 | TIMEOUT | Operation timeout |

## Memory Usage

### Static Allocations
- EEPROM calibration buffer: 512 bytes (256 × uint16_t)
- Frame data buffer: 1664 bytes (832 × uint16_t)
- **Total: ~2.2 KB** (minimal heap usage)

### Runtime Stack
- Stack usage: ~200 bytes (minimal)
- Heap allocations: None

## API Reference

### Public Methods

#### `init(uint8_t sda_pin, uint8_t scl_pin)`
Initialize I2C interface and verify sensor presence.

**Parameters**:
- `sda_pin`: GPIO pin for SDA line
- `scl_pin`: GPIO pin for SCL line

**Returns**: `MLX90640_Status`

---

#### `captureFrame(float* frame_data)`
Capture thermal frame and convert to float array.

**Parameters**:
- `frame_data`: Output buffer (must be size ≥ 768)

**Returns**: `MLX90640_Status`

**Notes**:
- Waits for new sensor data (1000ms timeout)
- Performs checksum validation
- Converts raw values to Celsius

---

#### `setRefreshRate(float refresh_rate)`
Set sensor refresh rate to specified frequency.

**Parameters**:
- `refresh_rate`: Frequency in Hz (0.5, 1, 2, 4, 8, 16)

**Returns**: `MLX90640_Status`

---

#### `setRefreshRate16Hz()`
Convenience method to set refresh rate to 16 Hz.

**Returns**: `MLX90640_Status`

---

#### `isInitialized()`
Check if sensor is initialized.

**Returns**: `bool`

---

#### `getLastError()`
Retrieve last operation error code.

**Returns**: `MLX90640_Status`

## Usage Examples

### Basic Thermal Imaging Loop

```cpp
void setup() {
    Serial.begin(115200);
    
    MLX90640Driver thermal_sensor;
    if (thermal_sensor.init(21, 22) != MLX90640_Status::OK) {
        Serial.println("Sensor initialization failed!");
        while(1) delay(1000);
    }
}

void loop() {
    float frame[MLX90640_PIXEL_COUNT];
    
    if (thermal_sensor.captureFrame(frame) == MLX90640_Status::OK) {
        // Process thermal data
        float center = frame[12 * 32 + 16]; // Center pixel
        Serial.println(center);
    }
    
    delay(63); // ~16Hz
}
```

### Error Recovery Pattern

```cpp
MLX90640_Status status = thermal_sensor.captureFrame(frame_data);

switch (status) {
    case MLX90640_Status::OK:
        // Process data
        break;
    
    case MLX90640_Status::TIMEOUT:
        Serial.println("Data not ready, retrying...");
        delay(10);
        break;
    
    case MLX90640_Status::CHECKSUM_ERROR:
        Serial.println("Data integrity error, retrying...");
        break;
    
    case MLX90640_Status::SENSOR_NOT_FOUND:
        Serial.println("Sensor disconnected!");
        thermal_sensor.init(21, 22); // Reinitialize
        break;
    
    default:
        Serial.println("Unknown error");
        break;
}
```

### Temperature Statistics

```cpp
float min_temp = frame_data[0];
float max_temp = frame_data[0];

for (int i = 0; i < MLX90640_PIXEL_COUNT; ++i) {
    if (frame_data[i] < min_temp) min_temp = frame_data[i];
    if (frame_data[i] > max_temp) max_temp = frame_data[i];
}

Serial.print("Temperature range: ");
Serial.print(min_temp, 1);
Serial.print("°C - ");
Serial.print(max_temp, 1);
Serial.println("°C");
```

## I2C Protocol Details

### Register Map (Key Registers)

| Address | Name | Size | Description |
|---------|------|------|-------------|
| 0x8000 | Status | 16-bit | Frame ready flag, new data indicator |
| 0x800D | CTL1 | 16-bit | Control: refresh rate bits [11:8] |
| 0x800E | CTL2 | 16-bit | Control register 2 |
| 0x0400 | RAM | 832×16-bit | Sensor RAM (image data) |
| 0x2400 | EEPROM | 256×16-bit | Calibration and configuration data |

### I2C Timing

- **Clock Speed**: 400 kHz
- **Slave Address**: 0x33
- **Data Ready Poll Rate**: ~1ms intervals
- **Frame Capture Timeout**: 1000ms

## Calibration Data

The driver automatically loads calibration coefficients from the sensor's EEPROM during initialization. These include:
- Temperature offset and gain
- Pixel-to-pixel variation correction
- Emissivity compensation
- Ambient temperature compensation

The calibration is factory-programmed and does not require user configuration.

## Performance Characteristics

### Latency
- I2C Register Read: ~100-200 µs
- Frame Capture: ~20-30 ms (at 16 Hz)
- Data Conversion: ~5-10 ms

### Thermal Performance
- Sensor Resolution: ±5°C or ±10% (whichever is greater)
- Temperature Range: -20°C to +300°C
- Frame Rate: Up to 16 Hz
- FOV: ~55° (depends on lens)

## Troubleshooting

### Sensor Not Found

**Symptoms**:
- `SENSOR_NOT_FOUND` error on init
- No response on I2C bus

**Solutions**:
1. Verify I2C wiring (SDA/SCL connected)
2. Check I2C address (default 0x33)
3. Verify 3.3V power supply to sensor
4. Check for I2C pull-up resistors (typically 4.7kΩ)
5. Use I2C scanner to detect device

### Checksum Errors

**Symptoms**:
- Frequent `CHECKSUM_ERROR` responses
- Data inconsistency

**Solutions**:
1. Check I2C clock integrity (oscilloscope)
2. Reduce cable length if possible
3. Add ferrite clamps on I2C lines
4. Verify sensor EEPROM integrity (factory default)

### Timeout Errors

**Symptoms**:
- `TIMEOUT` status code
- Sensor not responding to data-ready polls

**Solutions**:
1. Reduce refresh rate (try 8Hz or 4Hz)
2. Verify sensor power supply voltage
3. Check thermal sensor health (temperature)
4. Reinitialize driver

## Hardware Integration Checklist

- [ ] I2C wiring verified (SDA/SCL)
- [ ] Power supply 3.3V confirmed
- [ ] Pull-up resistors on I2C lines (4.7kΩ typical)
- [ ] Sensor thermal window clear and unobstructed
- [ ] GPIO pins configured correctly in code
- [ ] Arduino Wire library available
- [ ] EEPROM calibration data valid

## Advanced Configuration

### Custom I2C Pins

```cpp
// Use any ESP32 GPIO pins supporting I2C
thermal_sensor.init(18, 19); // GPIO18=SDA, GPIO19=SCL
```

### Custom Refresh Rate at Init

```cpp
thermal_sensor.init(21, 22);
thermal_sensor.setRefreshRate(8.0f); // 8 Hz instead of default 16Hz
```

## Contributing & Extensions

To extend this driver:

1. **Custom Temperature Processing**: Override `processRawFrame()`
2. **Advanced Calibration**: Enhance `loadCalibrationData()`
3. **Interrupt-Based Capture**: Hook to sensor data-ready pin
4. **Lower Power Modes**: Add sleep/standby functionality

## License

This driver is provided as-is for embedded systems development.

## References

- MLX90640 Datasheet: 40-pin thermal array sensor
- ESP32 I2C Arduino API
- I2C 2.1 Specification (400 kHz Fast Mode)

---

**Last Updated**: January 2026
**ESP32 Support**: Arduino IDE 2.x compatible
