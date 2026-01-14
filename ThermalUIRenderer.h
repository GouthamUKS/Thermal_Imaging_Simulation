#ifndef THERMAL_UI_RENDERER_H
#define THERMAL_UI_RENDERER_H

#include <cstdint>
#include <string>
#include "TargetLockTracker.h"

// ============================================================================
// UI Configuration
// ============================================================================

#define UI_DISPLAY_WIDTH 320
#define UI_DISPLAY_HEIGHT 240

// UI Elements
#define HUD_BORDER_THICKNESS 2
#define HUD_CORNER_SIZE 15
#define HUD_FONT_HEIGHT 8
#define HUD_PADDING 10

// ============================================================================
// Color Palette
// ============================================================================

namespace UIColors {
    // Primary colors
    constexpr uint32_t BLACK = 0x000000;
    constexpr uint32_t WHITE = 0xFFFFFF;
    constexpr uint32_t RED = 0xFF0000;
    constexpr uint32_t GREEN = 0x00FF00;
    constexpr uint32_t BLUE = 0x0000FF;
    constexpr uint32_t YELLOW = 0xFFFF00;
    constexpr uint32_t CYAN = 0x00FFFF;

    // Military theme
    constexpr uint32_t RETICLE_LOCKED = 0xFF0000;    // Red when locked
    constexpr uint32_t RETICLE_LOCKING = 0xFFFF00;   // Yellow while locking
    constexpr uint32_t RETICLE_SEARCHING = 0x00FF00; // Green searching
    constexpr uint32_t HUD_PRIMARY = 0x00FF00;       // Green HUD text
    constexpr uint32_t HUD_WARNING = 0xFFFF00;       // Yellow warnings
    constexpr uint32_t HUD_CRITICAL = 0xFF0000;      // Red critical

    // Thermal palette
    constexpr uint32_t THERMAL_COLD = 0x0000FF;
    constexpr uint32_t THERMAL_COOL = 0x00FFFF;
    constexpr uint32_t THERMAL_WARM = 0xFFFF00;
    constexpr uint32_t THERMAL_HOT = 0xFF0000;
    constexpr uint32_t THERMAL_SUPER_HOT = 0xFFFFFF;
}

// ============================================================================
// UI Renderer Class
// ============================================================================

/**
 * @class ThermalUIRenderer
 * @brief Sleek military-style thermal imaging display with HUD overlay
 */
class ThermalUIRenderer {
public:
    ThermalUIRenderer();
    ~ThermalUIRenderer() = default;

    /**
     * @brief Render complete thermal display with HUD
     * @param display_buffer Output framebuffer (320x240 RGB888)
     * @param thermal_data Upscaled thermal image data
     * @param tracker Target lock tracker for reticle and info
     */
    void renderDisplay(uint32_t* display_buffer, const float* thermal_data,
                      const TargetLockTracker& tracker);

    /**
     * @brief Render just the HUD overlay on existing framebuffer
     * @param display_buffer Framebuffer to draw on
     * @param tracker Target tracking data
     */
    void renderHUDOverlay(uint32_t* display_buffer,
                         const TargetLockTracker& tracker);

    /**
     * @brief Set HUD visibility
     * @param visible Show/hide HUD elements
     */
    void setHUDVisible(bool visible) { hud_visible_ = visible; }

    /**
     * @brief Set crosshair visibility
     * @param visible Show/hide center crosshair
     */
    void setCrosshairVisible(bool visible) { crosshair_visible_ = visible; }

    /**
     * @brief Toggle night vision effect (green monochrome)
     * @param enabled Enable night vision mode
     */
    void setNightVisionMode(bool enabled) { night_vision_ = enabled; }

    /**
     * @brief Set HUD opacity (for transparent overlay)
     * @param opacity 0.0 (transparent) to 1.0 (opaque)
     */
    void setHUDOpacity(float opacity) { hud_opacity_ = opacity; }

private:
    bool hud_visible_;
    bool crosshair_visible_;
    bool night_vision_;
    float hud_opacity_;

    /**
     * @brief Render thermal heatmap to framebuffer
     * @param display_buffer Output buffer
     * @param thermal_data Input thermal data
     */
    void renderThermalMap(uint32_t* display_buffer, const float* thermal_data);

    /**
     * @brief Draw center crosshair
     * @param display_buffer Framebuffer
     * @param color RGB888 color
     */
    void drawCenterCrosshair(uint32_t* display_buffer, uint32_t color);

    /**
     * @brief Draw corner HUD brackets
     * @param display_buffer Framebuffer
     * @param color RGB888 color
     */
    void drawCornerBrackets(uint32_t* display_buffer, uint32_t color);

    /**
     * @brief Draw lock state indicator
     * @param display_buffer Framebuffer
     * @param tracker Target tracking data
     */
    void drawLockStateIndicator(uint32_t* display_buffer,
                               const TargetLockTracker& tracker);

    /**
     * @brief Draw temperature information panel
     * @param display_buffer Framebuffer
     * @param tracker Target tracking data
     */
    void drawTemperaturePanel(uint32_t* display_buffer,
                             const TargetLockTracker& tracker);

    /**
     * @brief Draw scanning animation during search mode
     * @param display_buffer Framebuffer
     */
    void drawScanningAnimation(uint32_t* display_buffer);

    /**
     * @brief Draw pixel safely with bounds checking
     * @param display_buffer Framebuffer
     * @param x, y Coordinates
     * @param color RGB888 color
     */
    static void drawPixel(uint32_t* display_buffer, uint16_t x, uint16_t y,
                         uint32_t color);

    /**
     * @brief Draw filled rectangle
     * @param display_buffer Framebuffer
     * @param x0, y0, x1, y1 Rectangle bounds
     * @param color RGB888 color
     * @param filled True for filled, false for outline
     */
    static void drawRect(uint32_t* display_buffer, uint16_t x0, uint16_t y0,
                        uint16_t x1, uint16_t y1, uint32_t color, bool filled);

    /**
     * @brief Convert temperature to thermal display color
     * @param temperature Temperature in °C
     * @param min_temp Minimum temperature reference
     * @param max_temp Maximum temperature reference
     * @return RGB888 color value
     */
    static uint32_t temperatureToThermalColor(float temperature,
                                             float min_temp, float max_temp);

    /**
     * @brief Blend two colors with alpha
     * @param fg Foreground color
     * @param bg Background color
     * @param alpha Foreground opacity (0.0-1.0)
     * @return Blended RGB888 color
     */
    static uint32_t blendColors(uint32_t fg, uint32_t bg, float alpha);

    /**
     * @brief Draw simple text (monospace character)
     * @param display_buffer Framebuffer
     * @param x, y Starting position
     * @param text Text to draw
     * @param color RGB888 color
     */
    static void drawText(uint32_t* display_buffer, uint16_t x, uint16_t y,
                        const char* text, uint32_t color);
};

#endif // THERMAL_UI_RENDERER_H
