#ifndef MLX90640_TARGET_LOCK_H
#define MLX90640_TARGET_LOCK_H

#include <cstdint>
#include <cmath>

// ============================================================================
// Display Configuration
// ============================================================================

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define SENSOR_WIDTH 32
#define SENSOR_HEIGHT 24
#define UPSCALE_FACTOR_X (DISPLAY_WIDTH / SENSOR_WIDTH)   // 10
#define UPSCALE_FACTOR_Y (DISPLAY_HEIGHT / SENSOR_HEIGHT) // 10

// ============================================================================
// Reticle Configuration
// ============================================================================

#define RETICLE_SIZE 40           // Size of reticle square (pixels)
#define RETICLE_CORNER_LENGTH 12  // Length of corner brackets
#define RETICLE_THICKNESS 2       // Line thickness (pixels)

// ============================================================================
// Hysteresis and Filtering
// ============================================================================

#define HYSTERESIS_THRESHOLD 0.5f  // Temperature difference to trigger lock change (°C)
#define SMOOTHING_FACTOR 0.7f      // Exponential moving average factor (0-1)
#define MAX_JUMP_DISTANCE 50       // Max pixel distance before lock reset

/**
 * @enum LockState
 * @brief Target lock state machine
 */
enum class LockState : uint8_t {
    SEARCHING = 0,      // No valid target locked
    LOCKING = 1,        // Target candidate, gaining confidence
    LOCKED = 2,         // Target confirmed and tracking
    LOST = 3            // Previously locked target lost
};

/**
 * @enum ReticleStyle
 * @brief Visual style for targeting reticle
 */
enum class ReticleStyle : uint8_t {
    CORNER_BRACKETS = 0,  // Classic military style (recommended)
    CROSSHAIR = 1,        // Simple crosshair
    CIRCULAR = 2,         // Circular ring
    DIAMOND = 3           // Diamond shape
};

/**
 * @struct TargetPoint
 * @brief Represents a target location with associated metadata
 */
struct TargetPoint {
    float x;                // Display X coordinate (0-319)
    float y;                // Display Y coordinate (0-239)
    float temperature;      // Temperature at target (°C)
    float confidence;       // Confidence level (0.0-1.0)
    uint16_t frame_count;   // Frames since lock acquired
    bool is_valid;          // Valid target available
    
    TargetPoint() : x(0), y(0), temperature(0), confidence(0), 
                   frame_count(0), is_valid(false) {}
};

/**
 * @struct LockStatistics
 * @brief Statistics about current lock state
 */
struct LockStatistics {
    float current_temp;        // Current target temperature
    float max_temp_in_view;    // Maximum temperature in entire frame
    float min_temp_in_view;    // Minimum temperature in entire frame
    float temp_delta_threshold; // Current hysteresis threshold
    uint16_t total_hot_pixels; // Number of pixels above threshold
    uint16_t frame_age;        // Frames since lock acquired
    bool is_stable;            // Lock is stable (not jumping)
};

/**
 * @class TargetLockTracker
 * @brief Thermal target detection and tracking with reticle rendering
 *
 * Implements sophisticated target lock system suitable for:
 * - Defense/security thermal imaging
 * - Fire/rescue operations
 * - Industrial equipment monitoring
 * - Surveillance systems
 */
class TargetLockTracker {
public:
    TargetLockTracker();
    ~TargetLockTracker() = default;

    /**
     * @brief Process thermal frame and update target lock
     *
     * Scans thermal data for hottest point, applies hysteresis logic,
     * and updates lock state. Should be called once per thermal frame.
     *
     * @param thermal_data Input: 76,800-element upscaled thermal frame
     * @return TargetPoint with current lock information
     */
    TargetPoint processThermalFrame(const float* thermal_data);

    /**
     * @brief Get current lock state
     * @return Current LockState enumeration
     */
    LockState getLockState() const { return lock_state_; }

    /**
     * @brief Get current target location
     * @return TargetPoint with current target (or invalid if no lock)
     */
    TargetPoint getTarget() const { return current_target_; }

    /**
     * @brief Acquire lock on a specific coordinate (manual override)
     * @param x Display X coordinate
     * @param y Display Y coordinate
     * @param temperature Temperature at coordinate
     */
    void manualLock(float x, float y, float temperature);

    /**
     * @brief Release current lock (return to searching state)
     */
    void releaseLock();

    /**
     * @brief Enable/disable automatic tracking
     * @param enable true to enable auto-tracking, false for manual mode
     */
    void setAutoTracking(bool enable) { auto_tracking_ = enable; }

    /**
     * @brief Set reticle visual style
     * @param style ReticleStyle enumeration
     */
    void setReticleStyle(ReticleStyle style) { reticle_style_ = style; }

    /**
     * @brief Set hysteresis threshold (°C)
     * @param threshold Temperature difference to trigger lock change
     */
    void setHysteresisThreshold(float threshold) { hysteresis_threshold_ = threshold; }

    /**
     * @brief Set smoothing factor for position filtering
     * @param factor Exponential moving average factor (0.0-1.0)
     *               0.0 = no smoothing (raw), 1.0 = maximum smoothing
     */
    void setSmoothingFactor(float factor) {
        smoothing_factor_ = (factor < 0.0f) ? 0.0f : (factor > 1.0f) ? 1.0f : factor;
    }

    /**
     * @brief Set maximum allowable lock jump distance
     * @param distance Maximum pixels to allow between frames (prevents jitter)
     */
    void setMaxJumpDistance(float distance) { max_jump_distance_ = distance; }

    /**
     * @brief Get lock statistics for display/debugging
     * @param thermal_data Optional: thermal frame for stats calculation
     * @return LockStatistics structure
     */
    LockStatistics getStatistics(const float* thermal_data = nullptr) const;

    /**
     * @brief Reset tracking state (soft reset)
     */
    void reset();

    /**
     * @brief Get frame count since lock acquired
     * @return Number of frames in current lock
     */
    uint16_t getFramesSinceLock() const { return frame_counter_; }

public:
    // ========================================================================
    // Reticle Drawing Functions
    // ========================================================================

    /**
     * @brief Draw reticle on display frame buffer
     *
     * Renders the targeting reticle at current lock position.
     * Supports multiple visual styles.
     *
     * @param display_buffer Output: Display frame buffer (320x240 pixels)
     * @param pixel_format Display pixel format (0=RGB565, 1=RGB888, etc)
     * @return true if reticle drawn successfully
     */
    bool drawReticle(uint32_t* display_buffer, uint8_t pixel_format = 1);

    /**
     * @brief Draw reticle with custom color
     * @param display_buffer Output frame buffer
     * @param color_rgb 24-bit RGB color (0xRRGGBB)
     * @param pixel_format Display pixel format
     * @return true if successful
     */
    bool drawReticleCustomColor(uint32_t* display_buffer, uint32_t color_rgb,
                               uint8_t pixel_format = 1);

    /**
     * @brief Draw lock status information (text overlay)
     *
     * Renders temperature, confidence, and lock state as text.
     * Requires integration with display text rendering library.
     *
     * @param display_buffer Output frame buffer
     * @return true if text drawn successfully
     */
    bool drawLockStatus(uint32_t* display_buffer);

private:
    // State variables
    LockState lock_state_;
    TargetPoint current_target_;
    TargetPoint previous_target_;
    TargetPoint smoothed_target_;

    // Configuration
    ReticleStyle reticle_style_;
    float hysteresis_threshold_;
    float smoothing_factor_;
    float max_jump_distance_;
    bool auto_tracking_;

    // Tracking data
    uint16_t frame_counter_;
    uint16_t lock_stable_counter_;
    float max_temp_history_[10];  // Ring buffer for stability analysis
    uint8_t history_index_;

    // Internal methods

    /**
     * @brief Find hottest pixel in thermal data
     * @param thermal_data Input frame
     * @return TargetPoint at hottest location
     */
    TargetPoint findHottestPixel(const float* thermal_data);

    /**
     * @brief Apply hysteresis logic to prevent lock flicker
     * @param candidate New candidate target
     * @return true if candidate should trigger lock change
     */
    bool evaluateHysteresis(const TargetPoint& candidate);

    /**
     * @brief Apply positional smoothing (exponential moving average)
     * @param target Target to smooth
     * @return Smoothed target position
     */
    TargetPoint applySmoothingFilter(const TargetPoint& target);

    /**
     * @brief Check if target jump is valid (within max distance)
     * @param old_target Previous target
     * @param new_target Candidate target
     * @return true if jump is within acceptable range
     */
    bool isValidJump(const TargetPoint& old_target, const TargetPoint& new_target);

    /**
     * @brief Update lock confidence based on temperature stability
     * @param target Current target
     */
    void updateConfidence(TargetPoint& target);

    // Drawing helpers

    /**
     * @brief Draw single pixel to display buffer
     * @param buffer Frame buffer
     * @param x X coordinate
     * @param y Y coordinate
     * @param color RGB888 color
     */
    void setPixel(uint32_t* buffer, uint16_t x, uint16_t y, uint32_t color);

    /**
     * @brief Draw horizontal line
     */
    void drawLine(uint32_t* buffer, uint16_t x1, uint16_t y1, 
                 uint16_t x2, uint16_t y2, uint32_t color, uint8_t thickness = 1);

    /**
     * @brief Draw corner bracket reticle
     */
    void drawCornerBrackets(uint32_t* buffer, uint32_t color);

    /**
     * @brief Draw crosshair reticle
     */
    void drawCrosshair(uint32_t* buffer, uint32_t color);

    /**
     * @brief Draw circular reticle
     */
    void drawCircular(uint32_t* buffer, uint32_t color);

    /**
     * @brief Draw diamond reticle
     */
    void drawDiamond(uint32_t* buffer, uint32_t color);

    /**
     * @brief Get reticle color based on lock state
     * @return RGB888 color value
     */
    uint32_t getReticleColor() const;
};

#endif // MLX90640_TARGET_LOCK_H
