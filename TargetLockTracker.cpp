#include "TargetLockTracker.h"
#include <algorithm>

TargetLockTracker::TargetLockTracker()
    : lock_state_(LockState::SEARCHING),
      reticle_style_(ReticleStyle::CORNER_BRACKETS),
      hysteresis_threshold_(HYSTERESIS_THRESHOLD),
      smoothing_factor_(SMOOTHING_FACTOR),
      max_jump_distance_(MAX_JUMP_DISTANCE),
      auto_tracking_(true),
      frame_counter_(0),
      lock_stable_counter_(0),
      history_index_(0) {
    
    current_target_ = TargetPoint();
    previous_target_ = TargetPoint();
    smoothed_target_ = TargetPoint();
    
    // Initialize temperature history
    for (int i = 0; i < 10; ++i) {
        max_temp_history_[i] = 0.0f;
    }
}

TargetPoint TargetLockTracker::processThermalFrame(const float* thermal_data) {
    if (thermal_data == nullptr) {
        lock_state_ = LockState::LOST;
        return current_target_;
    }

    // Find hottest pixel in current frame
    TargetPoint candidate = findHottestPixel(thermal_data);
    
    if (!candidate.is_valid) {
        lock_state_ = LockState::LOST;
        return current_target_;
    }

    // State machine for lock management
    switch (lock_state_) {
        case LockState::SEARCHING: {
            // No current lock - evaluate if candidate should acquire lock
            if (auto_tracking_) {
                current_target_ = candidate;
                lock_state_ = LockState::LOCKING;
                frame_counter_ = 0;
                lock_stable_counter_ = 0;
                previous_target_ = candidate;
                smoothed_target_ = candidate;
            }
            break;
        }

        case LockState::LOCKING: {
            // Candidate lock - evaluate stability
            if (evaluateHysteresis(candidate)) {
                // Candidate confirmed, move to locked state
                lock_state_ = LockState::LOCKED;
                lock_stable_counter_ = 0;
            } else {
                // Still gathering confidence
                previous_target_ = current_target_;
                current_target_ = candidate;
            }
            break;
        }

        case LockState::LOCKED: {
            // Already locked - check if target should be updated or lost
            if (evaluateHysteresis(candidate)) {
                // Target update is valid
                previous_target_ = current_target_;
                current_target_ = candidate;
                lock_stable_counter_++;

                // Check if lock is stable
                if (lock_stable_counter_ > 3) {
                    updateConfidence(current_target_);
                }
            } else {
                // Large change detected - potential loss of target
                lock_stable_counter_ = 0;
                if (isValidJump(current_target_, candidate)) {
                    // Small enough jump to track
                    previous_target_ = current_target_;
                    current_target_ = candidate;
                } else {
                    // Jump too large - consider lock lost
                    lock_state_ = LockState::LOST;
                    current_target_.is_valid = false;
                    frame_counter_ = 0;
                }
            }
            break;
        }

        case LockState::LOST: {
            // Lost lock - revert to searching if new target strong enough
            if (candidate.temperature > (current_target_.temperature + hysteresis_threshold_)) {
                lock_state_ = LockState::SEARCHING;
                current_target_ = candidate;
                frame_counter_ = 0;
            }
            break;
        }

        default:
            break;
    }

    // Apply position smoothing
    smoothed_target_ = applySmoothingFilter(current_target_);

    // Increment frame counter
    frame_counter_++;

    return smoothed_target_;
}

void TargetLockTracker::manualLock(float x, float y, float temperature) {
    current_target_.x = x;
    current_target_.y = y;
    current_target_.temperature = temperature;
    current_target_.confidence = 0.95f; // Manual lock has high confidence
    current_target_.is_valid = true;
    current_target_.frame_count = 0;

    previous_target_ = current_target_;
    smoothed_target_ = current_target_;

    lock_state_ = LockState::LOCKED;
    frame_counter_ = 0;
    lock_stable_counter_ = 3; // Start as stable
}

void TargetLockTracker::releaseLock() {
    lock_state_ = LockState::SEARCHING;
    current_target_.is_valid = false;
    frame_counter_ = 0;
    lock_stable_counter_ = 0;
}

void TargetLockTracker::reset() {
    lock_state_ = LockState::SEARCHING;
    current_target_ = TargetPoint();
    previous_target_ = TargetPoint();
    smoothed_target_ = TargetPoint();
    frame_counter_ = 0;
    lock_stable_counter_ = 0;
    history_index_ = 0;
}

LockStatistics TargetLockTracker::getStatistics(const float* thermal_data) const {
    LockStatistics stats;
    stats.current_temp = current_target_.temperature;
    stats.temp_delta_threshold = hysteresis_threshold_;
    stats.frame_age = frame_counter_;
    stats.is_stable = (lock_stable_counter_ > 3);

    if (thermal_data != nullptr) {
        float max_temp = thermal_data[0];
        float min_temp = thermal_data[0];
        uint16_t hot_count = 0;
        float threshold = current_target_.temperature - 10.0f;

        for (int i = 1; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
            if (thermal_data[i] > max_temp) max_temp = thermal_data[i];
            if (thermal_data[i] < min_temp) min_temp = thermal_data[i];
            if (thermal_data[i] > threshold) hot_count++;
        }

        stats.max_temp_in_view = max_temp;
        stats.min_temp_in_view = min_temp;
        stats.total_hot_pixels = hot_count;
    }

    return stats;
}

TargetPoint TargetLockTracker::findHottestPixel(const float* thermal_data) {
    TargetPoint hottest;
    hottest.is_valid = false;
    hottest.temperature = -999.0f;
    hottest.confidence = 0.0f;

    uint32_t hottest_index = 0;

    // Scan entire upscaled thermal frame
    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        if (thermal_data[i] > hottest.temperature) {
            hottest.temperature = thermal_data[i];
            hottest_index = i;
            hottest.is_valid = true;
        }
    }

    if (!hottest.is_valid) {
        return hottest;
    }

    // Convert linear index to X, Y coordinates
    hottest.y = (float)(hottest_index / DISPLAY_WIDTH);
    hottest.x = (float)(hottest_index % DISPLAY_WIDTH);
    hottest.frame_count = 0;
    hottest.confidence = 0.5f; // Base confidence for new detection

    return hottest;
}

bool TargetLockTracker::evaluateHysteresis(const TargetPoint& candidate) {
    if (!current_target_.is_valid) {
        return true; // First target is always accepted
    }

    // Hysteresis logic: require temperature increase to break lock on cooler target
    // but allow tracking if current target increases
    float delta = candidate.temperature - current_target_.temperature;

    if (delta > hysteresis_threshold_) {
        // New target is significantly hotter - accept immediately
        return true;
    } else if (delta > -hysteresis_threshold_) {
        // Within hysteresis band - accept as update
        return true;
    } else {
        // New target is significantly cooler - reject
        return false;
    }
}

TargetPoint TargetLockTracker::applySmoothingFilter(const TargetPoint& target) {
    // Exponential moving average
    TargetPoint smoothed = target;

    if (previous_target_.is_valid && smoothing_factor_ > 0.01f) {
        smoothed.x = smoothing_factor_ * target.x + (1.0f - smoothing_factor_) * previous_target_.x;
        smoothed.y = smoothing_factor_ * target.y + (1.0f - smoothing_factor_) * previous_target_.y;
        smoothed.temperature = smoothing_factor_ * target.temperature +
                              (1.0f - smoothing_factor_) * previous_target_.temperature;
    }

    return smoothed;
}

bool TargetLockTracker::isValidJump(const TargetPoint& old_target, const TargetPoint& new_target) {
    if (!old_target.is_valid || !new_target.is_valid) {
        return false;
    }

    // Calculate Euclidean distance
    float dx = new_target.x - old_target.x;
    float dy = new_target.y - old_target.y;
    float distance = std::sqrt(dx * dx + dy * dy);

    return distance <= max_jump_distance_;
}

void TargetLockTracker::updateConfidence(TargetPoint& target) {
    // Add current temperature to history
    max_temp_history_[history_index_] = target.temperature;
    history_index_ = (history_index_ + 1) % 10;

    // Calculate variance to assess stability
    float mean = 0.0f;
    for (int i = 0; i < 10; ++i) {
        mean += max_temp_history_[i];
    }
    mean /= 10.0f;

    float variance = 0.0f;
    for (int i = 0; i < 10; ++i) {
        float delta = max_temp_history_[i] - mean;
        variance += delta * delta;
    }
    variance /= 10.0f;

    // Stable signal = low variance = high confidence
    float std_dev = std::sqrt(variance);
    target.confidence = 1.0f - std::min(std_dev / 5.0f, 1.0f);
}

// ============================================================================
// Display/Graphics Functions
// ============================================================================

bool TargetLockTracker::drawReticle(uint32_t* display_buffer, uint8_t pixel_format) {
    if (display_buffer == nullptr || !current_target_.is_valid) {
        return false;
    }

    uint32_t color = getReticleColor();

    switch (reticle_style_) {
        case ReticleStyle::CORNER_BRACKETS:
            drawCornerBrackets(display_buffer, color);
            break;
        case ReticleStyle::CROSSHAIR:
            drawCrosshair(display_buffer, color);
            break;
        case ReticleStyle::CIRCULAR:
            drawCircular(display_buffer, color);
            break;
        case ReticleStyle::DIAMOND:
            drawDiamond(display_buffer, color);
            break;
        default:
            return false;
    }

    return true;
}

bool TargetLockTracker::drawReticleCustomColor(uint32_t* display_buffer, uint32_t color_rgb,
                                              uint8_t pixel_format) {
    if (display_buffer == nullptr || !current_target_.is_valid) {
        return false;
    }

    switch (reticle_style_) {
        case ReticleStyle::CORNER_BRACKETS:
            drawCornerBrackets(display_buffer, color_rgb);
            break;
        case ReticleStyle::CROSSHAIR:
            drawCrosshair(display_buffer, color_rgb);
            break;
        case ReticleStyle::CIRCULAR:
            drawCircular(display_buffer, color_rgb);
            break;
        case ReticleStyle::DIAMOND:
            drawDiamond(display_buffer, color_rgb);
            break;
        default:
            return false;
    }

    return true;
}

bool TargetLockTracker::drawLockStatus(uint32_t* display_buffer) {
    if (display_buffer == nullptr) {
        return false;
    }

    // Text rendering would require integration with display library
    // This is a placeholder for status information display
    return true;
}

void TargetLockTracker::setPixel(uint32_t* buffer, uint16_t x, uint16_t y, uint32_t color) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        return;
    }

    uint32_t index = y * DISPLAY_WIDTH + x;
    buffer[index] = color;
}

void TargetLockTracker::drawLine(uint32_t* buffer, uint16_t x1, uint16_t y1,
                                uint16_t x2, uint16_t y2, uint32_t color, uint8_t thickness) {
    // Bresenham line algorithm for efficient line drawing
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int x = x1;
    int y = y1;

    while (true) {
        // Draw with thickness
        for (int t = 0; t < thickness; ++t) {
            setPixel(buffer, x + t, y, color);
            setPixel(buffer, x, y + t, color);
        }

        if (x == x2 && y == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void TargetLockTracker::drawCornerBrackets(uint32_t* buffer, uint32_t color) {
    /**
     * Classic military-style reticle with corner brackets
     *
     * Example at (160, 120) with size 40:
     *     +---+           +---+
     *     |               |
     *     |               |
     *  +--+               +--+
     */

    int cx = (int)smoothed_target_.x;
    int cy = (int)smoothed_target_.y;
    int size = RETICLE_SIZE;
    int corner_len = RETICLE_CORNER_LENGTH;
    int thickness = RETICLE_THICKNESS;

    // Top-left corner
    drawLine(buffer, cx - size/2, cy - size/2, 
            cx - size/2 + corner_len, cy - size/2, color, thickness);
    drawLine(buffer, cx - size/2, cy - size/2, 
            cx - size/2, cy - size/2 + corner_len, color, thickness);

    // Top-right corner
    drawLine(buffer, cx + size/2, cy - size/2, 
            cx + size/2 - corner_len, cy - size/2, color, thickness);
    drawLine(buffer, cx + size/2, cy - size/2, 
            cx + size/2, cy - size/2 + corner_len, color, thickness);

    // Bottom-left corner
    drawLine(buffer, cx - size/2, cy + size/2, 
            cx - size/2 + corner_len, cy + size/2, color, thickness);
    drawLine(buffer, cx - size/2, cy + size/2, 
            cx - size/2, cy + size/2 - corner_len, color, thickness);

    // Bottom-right corner
    drawLine(buffer, cx + size/2, cy + size/2, 
            cx + size/2 - corner_len, cy + size/2, color, thickness);
    drawLine(buffer, cx + size/2, cy + size/2, 
            cx + size/2, cy + size/2 - corner_len, color, thickness);

    // Center crosshair dot
    setPixel(buffer, cx, cy, color);
    setPixel(buffer, cx + 1, cy, color);
    setPixel(buffer, cx, cy + 1, color);
    setPixel(buffer, cx + 1, cy + 1, color);
}

void TargetLockTracker::drawCrosshair(uint32_t* buffer, uint32_t color) {
    /**
     * Simple crosshair targeting reticle
     *
     *        |
     *     ---+---
     *        |
     */

    int cx = (int)smoothed_target_.x;
    int cy = (int)smoothed_target_.y;
    int size = RETICLE_SIZE;
    int thickness = RETICLE_THICKNESS;

    // Vertical line
    drawLine(buffer, cx, cy - size/2, cx, cy + size/2, color, thickness);

    // Horizontal line
    drawLine(buffer, cx - size/2, cy, cx + size/2, cy, color, thickness);

    // Center dot
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            setPixel(buffer, cx + x, cy + y, color);
        }
    }
}

void TargetLockTracker::drawCircular(uint32_t* buffer, uint32_t color) {
    /**
     * Circular ring reticle using Bresenham circle algorithm
     */

    int cx = (int)smoothed_target_.x;
    int cy = (int)smoothed_target_.y;
    int radius = RETICLE_SIZE / 2;

    // Midpoint circle algorithm
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    while (x <= y) {
        // Draw 8 octants
        setPixel(buffer, cx + x, cy + y, color);
        setPixel(buffer, cx - x, cy + y, color);
        setPixel(buffer, cx + x, cy - y, color);
        setPixel(buffer, cx - x, cy - y, color);
        setPixel(buffer, cx + y, cy + x, color);
        setPixel(buffer, cx - y, cy + x, color);
        setPixel(buffer, cx + y, cy - x, color);
        setPixel(buffer, cx - y, cy - x, color);

        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

    // Center dot
    setPixel(buffer, cx, cy, color);
}

void TargetLockTracker::drawDiamond(uint32_t* buffer, uint32_t color) {
    /**
     * Diamond-shaped reticle
     *
     *       /\
     *      /  \
     *     |    |
     *      \  /
     *       \/
     */

    int cx = (int)smoothed_target_.x;
    int cy = (int)smoothed_target_.y;
    int size = RETICLE_SIZE / 2;

    // Top to right
    drawLine(buffer, cx, cy - size, cx + size, cy, color, RETICLE_THICKNESS);
    // Right to bottom
    drawLine(buffer, cx + size, cy, cx, cy + size, color, RETICLE_THICKNESS);
    // Bottom to left
    drawLine(buffer, cx, cy + size, cx - size, cy, color, RETICLE_THICKNESS);
    // Left to top
    drawLine(buffer, cx - size, cy, cx, cy - size, color, RETICLE_THICKNESS);

    // Center dot
    setPixel(buffer, cx, cy, color);
}

uint32_t TargetLockTracker::getReticleColor() const {
    // Color based on lock state
    switch (lock_state_) {
        case LockState::SEARCHING:
            return 0x00FF00; // Green (searching)

        case LockState::LOCKING:
            return 0xFFFF00; // Yellow (locking)

        case LockState::LOCKED: {
            // Green if confident, yellow if less confident
            if (current_target_.confidence > 0.7f) {
                return 0x00FF00; // Bright green
            } else {
                return 0xFFFF00; // Yellow
            }
        }

        case LockState::LOST:
            return 0xFF0000; // Red (lost)

        default:
            return 0xFFFFFF; // White (unknown)
    }
}
