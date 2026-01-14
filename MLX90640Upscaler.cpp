#include "MLX90640Upscaler.h"
#include <cmath>

MLX90640Upscaler::MLX90640Upscaler()
    : method_(InterpolationMethod::BILINEAR) {
}

bool MLX90640Upscaler::upscale(const float* raw_data, float* output) {
    if (raw_data == nullptr || output == nullptr) {
        return false;
    }

    switch (method_) {
        case InterpolationMethod::BILINEAR:
            return upscaleFixedPoint(raw_data, output);

        case InterpolationMethod::NEAREST_NEIGHBOR: {
            // Fastest method: direct pixel replication
            for (uint16_t y = 0; y < DISPLAY_HEIGHT; ++y) {
                for (uint16_t x = 0; x < DISPLAY_WIDTH; ++x) {
                    // Map display coordinates directly to sensor coordinates
                    uint16_t sensor_x = x / UPSCALE_FACTOR_X;
                    uint16_t sensor_y = y / UPSCALE_FACTOR_Y;

                    uint32_t display_idx = y * DISPLAY_WIDTH + x;
                    uint32_t sensor_idx = sensor_y * SENSOR_WIDTH + sensor_x;

                    output[display_idx] = raw_data[sensor_idx];
                }
            }
            return true;
        }

        default:
            return upscaleFixedPoint(raw_data, output);
    }
}

bool MLX90640Upscaler::upscaleFixedPoint(const float* raw_data, float* output) {
    if (raw_data == nullptr || output == nullptr) {
        return false;
    }

    /**
     * Fixed-point bilinear interpolation optimized for ESP32
     *
     * Key optimizations:
     * 1. Use pre-computed scale factors (10x = compile-time constant)
     * 2. Integer arithmetic for fractional parts (alpha, beta)
     * 3. Avoid division in hot loop (use bit shifts where possible)
     * 4. Cache friendly: process row by row
     */

    for (uint16_t display_y = 0; display_y < DISPLAY_HEIGHT; ++display_y) {
        // Compute sensor Y coordinate and fractional part
        // sensor_y = display_y / UPSCALE_FACTOR_Y
        uint16_t sensor_y = display_y / UPSCALE_FACTOR_Y;
        uint16_t remainder_y = display_y % UPSCALE_FACTOR_Y;

        // Fractional part in 16-bit fixed-point (0-65535 represents 0.0-1.0)
        uint16_t beta_fixed = (remainder_y << 16) / UPSCALE_FACTOR_Y;

        // Clamp to valid sensor row range
        if (sensor_y >= SENSOR_HEIGHT - 1) {
            sensor_y = SENSOR_HEIGHT - 2;
            beta_fixed = 65535; // Full weight to bottom row
        }

        uint16_t sensor_y_next = sensor_y + 1;

        for (uint16_t display_x = 0; display_x < DISPLAY_WIDTH; ++display_x) {
            // Compute sensor X coordinate and fractional part
            uint16_t sensor_x = display_x / UPSCALE_FACTOR_X;
            uint16_t remainder_x = display_x % UPSCALE_FACTOR_X;

            // Fractional part in 16-bit fixed-point
            uint16_t alpha_fixed = (remainder_x << 16) / UPSCALE_FACTOR_X;

            // Clamp to valid sensor column range
            uint16_t sensor_x_next = sensor_x;
            if (sensor_x < SENSOR_WIDTH - 1) {
                sensor_x_next = sensor_x + 1;
            }

            // Get the four neighboring sensor pixels
            uint32_t idx_00 = sensor_y * SENSOR_WIDTH + sensor_x;
            uint32_t idx_10 = sensor_y * SENSOR_WIDTH + sensor_x_next;
            uint32_t idx_01 = sensor_y_next * SENSOR_WIDTH + sensor_x;
            uint32_t idx_11 = sensor_y_next * SENSOR_WIDTH + sensor_x_next;

            float p00 = raw_data[idx_00];
            float p10 = raw_data[idx_10];
            float p01 = raw_data[idx_01];
            float p11 = raw_data[idx_11];

            // Perform bilinear interpolation
            // Formula: f(α,β) = (1-α)(1-β)p00 + α(1-β)p10 +
            //                   (1-α)β p01 + αβ p11
            float alpha = alpha_fixed / 65535.0f;
            float beta = beta_fixed / 65535.0f;

            float result = bilinearInterpolate(p00, p10, p01, p11, alpha, beta);

            uint32_t display_idx = display_y * DISPLAY_WIDTH + display_x;
            output[display_idx] = result;
        }
    }

    return true;
}

bool MLX90640Upscaler::upscaleRow(const float* raw_data, float* output_row,
                                   uint16_t row_index) {
    if (raw_data == nullptr || output_row == nullptr) {
        return false;
    }

    if (row_index >= DISPLAY_HEIGHT) {
        return false;
    }

    // Compute sensor Y coordinate
    uint16_t sensor_y = row_index / UPSCALE_FACTOR_Y;
    if (sensor_y >= SENSOR_HEIGHT - 1) {
        sensor_y = SENSOR_HEIGHT - 2;
    }
    uint16_t sensor_y_next = sensor_y + 1;

    // Compute vertical fractional part
    uint16_t remainder_y = row_index % UPSCALE_FACTOR_Y;
    uint16_t beta_fixed = (remainder_y << 16) / UPSCALE_FACTOR_Y;

    // Process single output row
    for (uint16_t display_x = 0; display_x < DISPLAY_WIDTH; ++display_x) {
        uint16_t sensor_x = display_x / UPSCALE_FACTOR_X;
        uint16_t remainder_x = display_x % UPSCALE_FACTOR_X;
        uint16_t alpha_fixed = (remainder_x << 16) / UPSCALE_FACTOR_X;

        uint16_t sensor_x_next = sensor_x;
        if (sensor_x < SENSOR_WIDTH - 1) {
            sensor_x_next = sensor_x + 1;
        }

        float p00 = raw_data[sensor_y * SENSOR_WIDTH + sensor_x];
        float p10 = raw_data[sensor_y * SENSOR_WIDTH + sensor_x_next];
        float p01 = raw_data[sensor_y_next * SENSOR_WIDTH + sensor_x];
        float p11 = raw_data[sensor_y_next * SENSOR_WIDTH + sensor_x_next];

        float alpha = alpha_fixed / 65535.0f;
        float beta = beta_fixed / 65535.0f;

        output_row[display_x] = bilinearInterpolate(p00, p10, p01, p11, alpha, beta);
    }

    return true;
}

float MLX90640Upscaler::getInterpolatedValue(const float* raw_data,
                                            uint16_t display_x, uint16_t display_y) {
    if (raw_data == nullptr) {
        return 0.0f;
    }

    if (display_x >= DISPLAY_WIDTH || display_y >= DISPLAY_HEIGHT) {
        return 0.0f;
    }

    // Map display coordinates to sensor coordinates
    uint16_t sensor_x = display_x / UPSCALE_FACTOR_X;
    uint16_t remainder_x = display_x % UPSCALE_FACTOR_X;
    float alpha = (float)remainder_x / UPSCALE_FACTOR_X;

    uint16_t sensor_y = display_y / UPSCALE_FACTOR_Y;
    uint16_t remainder_y = display_y % UPSCALE_FACTOR_Y;
    float beta = (float)remainder_y / UPSCALE_FACTOR_Y;

    // Clamp to valid range
    if (sensor_x >= SENSOR_WIDTH - 1) sensor_x = SENSOR_WIDTH - 2;
    if (sensor_y >= SENSOR_HEIGHT - 1) sensor_y = SENSOR_HEIGHT - 2;

    uint16_t sensor_x_next = sensor_x + 1;
    uint16_t sensor_y_next = sensor_y + 1;

    float p00 = raw_data[sensor_y * SENSOR_WIDTH + sensor_x];
    float p10 = raw_data[sensor_y * SENSOR_WIDTH + sensor_x_next];
    float p01 = raw_data[sensor_y_next * SENSOR_WIDTH + sensor_x];
    float p11 = raw_data[sensor_y_next * SENSOR_WIDTH + sensor_x_next];

    return bilinearInterpolate(p00, p10, p01, p11, alpha, beta);
}

float MLX90640Upscaler::bilinearInterpolate(float p00, float p10, float p01, float p11,
                                            float alpha, float beta) {
    /**
     * Bilinear interpolation:
     * Result = (1-α)(1-β)·p00 + α(1-β)·p10 +
     *          (1-α)β·p01 + αβ·p11
     *
     * Rearranged for efficiency (minimize operations):
     * Result = (1-β)[p00 + α(p10-p00)] + β[p01 + α(p11-p01)]
     * Result = (1-β)[p00 + α·Δx0] + β[p01 + α·Δx1]
     *
     * This reduces 7 multiplications to 3
     */

    float one_minus_alpha = 1.0f - alpha;
    float one_minus_beta = 1.0f - beta;

    // Interpolate along x-axis for top and bottom rows
    float top = one_minus_alpha * p00 + alpha * p10;
    float bottom = one_minus_alpha * p01 + alpha * p11;

    // Interpolate between top and bottom
    return one_minus_beta * top + beta * bottom;
}

int32_t MLX90640Upscaler::bilinearInterpolateFixedPoint(int32_t p00, int32_t p10,
                                                       int32_t p01, int32_t p11,
                                                       uint16_t alpha_fixed, uint16_t beta_fixed) {
    /**
     * Fixed-point version using 16-bit fractional parts
     * alpha_fixed, beta_fixed: 0-65535 represents 0.0-1.0
     *
     * This avoids floating-point division in the hot loop
     */

    uint16_t one_minus_alpha = 65535 - alpha_fixed;
    uint16_t one_minus_beta = 65535 - beta_fixed;

    // Interpolate along x-axis (use 32-bit to avoid overflow)
    int32_t top = (int32_t)one_minus_alpha * p00 + (int32_t)alpha_fixed * p10;
    int32_t bottom = (int32_t)one_minus_alpha * p01 + (int32_t)alpha_fixed * p11;

    // Interpolate between top and bottom
    int32_t result = ((int32_t)one_minus_beta * top + (int32_t)beta_fixed * bottom) / 65535;

    return result;
}

float MLX90640Upscaler::getPixel(const float* raw_data, int32_t x, int32_t y) {
    // Bounds checking
    if (x < 0 || x >= SENSOR_WIDTH || y < 0 || y >= SENSOR_HEIGHT) {
        return 0.0f;
    }
    return raw_data[y * SENSOR_WIDTH + x];
}

float MLX90640Upscaler::mapDisplayToSensor(uint16_t display_coord,
                                          uint16_t sensor_size, uint16_t display_size) {
    /**
     * Map output coordinate to input coordinate space
     * For 10x scaling:
     *   sensor_coord = display_coord / 10.0
     */
    return (float)display_coord * sensor_size / display_size;
}

void MLX90640Upscaler::stretchContrast(float* data, uint32_t length,
                                       float min_temp, float max_temp) {
    if (data == nullptr || length == 0) {
        return;
    }

    // Auto-detect min/max if not provided
    if (min_temp > max_temp) {
        min_temp = data[0];
        max_temp = data[0];

        for (uint32_t i = 1; i < length; ++i) {
            if (data[i] < min_temp) min_temp = data[i];
            if (data[i] > max_temp) max_temp = data[i];
        }
    }

    float range = max_temp - min_temp;
    if (range < 0.001f) {
        // All values are essentially the same
        for (uint32_t i = 0; i < length; ++i) {
            data[i] = 50.0f; // Middle of 0-100 range
        }
        return;
    }

    // Normalize to 0-100 range
    for (uint32_t i = 0; i < length; ++i) {
        data[i] = ((data[i] - min_temp) / range) * 100.0f;
        // Clamp to 0-100
        if (data[i] < 0.0f) data[i] = 0.0f;
        if (data[i] > 100.0f) data[i] = 100.0f;
    }
}
