# Thermal Imaging Simulation

A thermal imaging system using the MLX90640 infrared sensor with ESP32.

## Features
- MLX90640 thermal camera driver
- Real-time temperature data capture
- Configurable refresh rates (0.5Hz - 16Hz)
- I2C communication interface

## Hardware Requirements
- ESP32 development board
- MLX90640 thermal imaging sensor
- I2C connection (SDA/SCL pins)

## Building
```bash
mkdir build
cd build
cmake ..
make
```

## Usage
Initialize the sensor and capture thermal frames for processing.

## License
TBD
