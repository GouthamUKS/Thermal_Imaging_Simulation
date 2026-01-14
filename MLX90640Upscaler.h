#ifndef MLX90640_UPSCALER_H
#define MLX90640_UPSCALER_H

#include <cstdint>
#include <cstring>

// ============================================================================
// Configuration: Upscaling Parameters
// ============================================================================

// Input sensor dimensions (MLX90640)
#define SENSOR_WIDTH 32
#define SENSOR_HEIGHT 24
#define SENSOR_PIXELS (SENSOR_WIDTH * SENSOR_HEIGHT)

// Output display dimensions
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define DISPLAY_PIXELS (DISPLAY_WIDTH * DISPLAY_HEIGHT)

// Scale factor (must be integer for optimal performance)
#define UPSCALE_FACTOR_X (DISPLAY_WIDTH / SENSOR_WIDTH)   // 10
#define UPSCALE_FACTOR_Y (DISPLAY_HEIGHT / SENSOR_HEIGHT) // 10

// Data type for intermediate calculations
// Use uint16_t for fixed-point to avoid float operations
#define TEMPERATURE_SCALE 100  // Store as integer * 100 for 2 decimal precision

/**
 * @enum InterpolationMethod
 * @brief Available interpolation algorithms
 */
enum class InterpolationMethod : uint8_t {
    BILINEAR = 0,      // Bilinear interpolation (recommended)
    NEAREST_NEIGHBOR = 1, // Fastest, blockiest
    CUBIC = 2          // Smoother, slower (not recommended for ESP32)
};

/**
 * @class MLX90640Upscaler
 * @brief Bilinear interpolation upscaler for thermal image data
 *
 * Converts 32x24 thermal sensor data to smooth 320x240 display output
 * Optimized for ESP32 with minimal floating-point operations
 */
class MLX90640Upscaler {
public:
    MLX90640Upscaler();
    ~MLX90640Upscaler() = default;

    /**
     * @brief Upscale thermal data using bilinear interpolation
     *
     * Takes raw 32x24 sensor data and produces smooth 320x240 output
     * using bilinear interpolation for subpixel accuracy.
     *
     * @param raw_data Input: 768-element array from MLX90640 (in °C as floats)
     * @param output Display Output: 76800-element array for 320x240 display
     * @return true if successful, false on error
     */
    bool upscale(const float* raw_data, float* output);

    /**
     * @brief Upscale with fixed-point arithmetic (faster, integer-only)
     *
     * Alternative implementation using fixed-point math to avoid floating-point
     * operations. Uses Q16 format (16-bit fractional part).
     *
     * @param raw_data Input: 768-element array (floats converted to fixed-point)
     * @param output Display Output: 76800-element array
     * @return true if successful
     */
    bool upscaleFixedPoint(const float* raw_data, float* output);

    /**
     * @brief Upscale single row of thermal data (for streaming)
     *
     * Process one output row at a time for memory-constrained systems.
     * Useful for line-by-line display updates on 320x240 LCD.
     *
     * @param raw_data Input: full 768-element sensor array
     * @param output_row Output: 320-element array for one display row
     * @param row_index Output row index (0-239)
     * @return true if successful
     */
    bool upscaleRow(const float* raw_data, float* output_row, uint16_t row_index);

    /**
     * @brief Get interpolated value at arbitrary position
     *
     * Compute bilinear interpolation at any point in output grid
     * without upscaling entire array.
     *
     * @param raw_data Input: 768-element sensor array
     * @param display_x X coordinate in display (0-319)
     * @param display_y Y coordinate in display (0-239)
     * @return Interpolated temperature value
     */
    float getInterpolatedValue(const float* raw_data, uint16_t display_x, uint16_t display_y);

    /**
     * @brief Set interpolation method
     * @param method InterpolationMethod enum value
     */
    void setInterpolationMethod(InterpolationMethod method) { method_ = method; }

    /**
     * @brief Get interpolation method
     * @return Current interpolation method
     */
    InterpolationMethod getInterpolationMethod() const { return method_; }

    /**
     * @brief Apply contrast stretching (normalize data to 0-100 range)
     *
     * Enhances visibility by stretching to full dynamic range.
     * Useful after upscaling for display.
     *
     * @param data Array to stretch (modified in-place)
     * @param length Number of elements
     * @param min_temp Minimum temperature reference (or auto-detect if 999)
     * @param max_temp Maximum temperature reference (or auto-detect if 999)
     */
    static void stretchContrast(float* data, uint32_t length, 
                               float min_temp = 999.0f, float max_temp = -999.0f);

private:
    InterpolationMethod method_;

    /**
     * @brief Core bilinear interpolation kernel
     *
     * Performs bilinear interpolation on 4 neighboring sensor pixels.
     * Formula:
     *   f(x,y) = (1-α)(1-β)·p00 + α(1-β)·p10 +
     *            (1-α)β·p01 + αβ·p11
     * where α,β ∈ [0,1] are fractional positions
     *
     * @param p00 Top-left pixel value
     * @param p10 Top-right pixel value
     * @param p01 Bottom-left pixel value
     * @param p11 Bottom-right pixel value
     * @param alpha Horizontal interpolation factor (0.0-1.0)
     * @param beta Vertical interpolation factor (0.0-1.0)
     * @return Interpolated value
     */
    static float bilinearInterpolate(float p00, float p10, float p01, float p11,
                                     float alpha, float beta);

    /**
     * @brief Fixed-point bilinear interpolation (integer-only)
     *
     * Uses Q16 fixed-point arithmetic for speed on embedded systems.
     * Avoids floating-point division in tight loops.
     *
     * @param p00 Top-left (as int32_t in Q16 format)
     * @param p10 Top-right
     * @param p01 Bottom-left
     * @param p11 Bottom-right
     * @param alpha_fixed Horizontal factor (as 16-bit fraction, 0-65536)
     * @param beta_fixed Vertical factor (as 16-bit fraction, 0-65536)
     * @return Interpolated value (as int32_t in Q16, convert back to float)
     */
    static int32_t bilinearInterpolateFixedPoint(int32_t p00, int32_t p10, 
                                                 int32_t p01, int32_t p11,
                                                 uint16_t alpha_fixed, uint16_t beta_fixed);

    /**
     * @brief Get sensor pixel value with bounds checking
     * @param raw_data Input array
     * @param x X coordinate (0-31)
     * @param y Y coordinate (0-23)
     * @return Temperature value, or 0 if out of bounds
     */
    static float getPixel(const float* raw_data, int32_t x, int32_t y);

    /**
     * @brief Convert display coordinates to sensor coordinates
     * @param display_coord Output grid coordinate
     * @param sensor_size Input grid size (32 or 24)
     * @param display_size Output grid size (320 or 240)
     * @return Fractional sensor coordinate
     */
    static float mapDisplayToSensor(uint16_t display_coord, 
                                    uint16_t sensor_size, uint16_t display_size);
};

#endif // MLX90640_UPSCALER_H
