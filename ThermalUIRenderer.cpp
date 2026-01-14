#include "ThermalUIRenderer.h"
#include <cmath>
#include <cstdio>

ThermalUIRenderer::ThermalUIRenderer()
    : hud_visible_(true),
      crosshair_visible_(true),
      night_vision_(false),
      hud_opacity_(1.0f) {
}

void ThermalUIRenderer::renderDisplay(uint32_t* display_buffer,
                                     const float* thermal_data,
                                     const TargetLockTracker& tracker) {
    if (display_buffer == nullptr || thermal_data == nullptr) {
        return;
    }

    // Step 1: Render thermal heatmap
    renderThermalMap(display_buffer, thermal_data);

    // Step 2: Draw targeting reticle
    const_cast<TargetLockTracker&>(tracker).drawReticle(display_buffer);

    // Step 3: Render HUD overlay
    if (hud_visible_) {
        renderHUDOverlay(display_buffer, tracker);
    }

    // Step 4: Draw center crosshair
    if (crosshair_visible_) {
        drawCenterCrosshair(display_buffer, UIColors::HUD_PRIMARY);
    }
}

void ThermalUIRenderer::renderHUDOverlay(uint32_t* display_buffer,
                                        const TargetLockTracker& tracker) {
    if (display_buffer == nullptr) {
        return;
    }

    // Draw corner HUD brackets
    drawCornerBrackets(display_buffer, UIColors::HUD_PRIMARY);

    // Draw lock state indicator (top-left)
    drawLockStateIndicator(display_buffer, tracker);

    // Draw temperature information panel (bottom-right)
    drawTemperaturePanel(display_buffer, tracker);

    // Draw scanning animation if searching
    if (tracker.getLockState() == LockState::SEARCHING) {
        drawScanningAnimation(display_buffer);
    }
}

void ThermalUIRenderer::renderThermalMap(uint32_t* display_buffer,
                                        const float* thermal_data) {
    for (uint16_t y = 0; y < UI_DISPLAY_HEIGHT; ++y) {
        for (uint16_t x = 0; x < UI_DISPLAY_WIDTH; ++x) {
            uint32_t idx = y * UI_DISPLAY_WIDTH + x;
            float temp = thermal_data[idx];

            // Convert to display color
            uint32_t color = temperatureToThermalColor(temp, -20.0f, 300.0f);

            if (night_vision_) {
                // Convert to green monochrome for night vision
                uint8_t r = (color >> 16) & 0xFF;
                uint8_t g = (color >> 8) & 0xFF;
                uint8_t b = color & 0xFF;
                uint8_t brightness = (r + g + g + b) / 4;  // Green-weighted
                color = (brightness << 8);  // Green channel only
            }

            display_buffer[idx] = color;
        }
    }
}

void ThermalUIRenderer::drawCenterCrosshair(uint32_t* display_buffer,
                                           uint32_t color) {
    uint16_t center_x = UI_DISPLAY_WIDTH / 2;
    uint16_t center_y = UI_DISPLAY_HEIGHT / 2;
    uint16_t size = 10;

    // Horizontal line
    for (int16_t x = center_x - size; x <= center_x + size; ++x) {
        if (x >= 0 && x < UI_DISPLAY_WIDTH) {
            drawPixel(display_buffer, x, center_y, color);
        }
    }

    // Vertical line
    for (int16_t y = center_y - size; y <= center_y + size; ++y) {
        if (y >= 0 && y < UI_DISPLAY_HEIGHT) {
            drawPixel(display_buffer, center_x, y, color);
        }
    }

    // Center dot
    drawPixel(display_buffer, center_x, center_y, color);
}

void ThermalUIRenderer::drawCornerBrackets(uint32_t* display_buffer,
                                          uint32_t color) {
    int16_t size = HUD_CORNER_SIZE;
    int16_t thickness = HUD_BORDER_THICKNESS;

    // Top-left corner
    drawRect(display_buffer, HUD_PADDING, HUD_PADDING,
            HUD_PADDING + size, HUD_PADDING + thickness, color, true);
    drawRect(display_buffer, HUD_PADDING, HUD_PADDING,
            HUD_PADDING + thickness, HUD_PADDING + size, color, true);

    // Top-right corner
    drawRect(display_buffer, UI_DISPLAY_WIDTH - HUD_PADDING - size, HUD_PADDING,
            UI_DISPLAY_WIDTH - HUD_PADDING, HUD_PADDING + thickness, color, true);
    drawRect(display_buffer, UI_DISPLAY_WIDTH - HUD_PADDING - thickness, HUD_PADDING,
            UI_DISPLAY_WIDTH - HUD_PADDING, HUD_PADDING + size, color, true);

    // Bottom-left corner
    drawRect(display_buffer, HUD_PADDING, UI_DISPLAY_HEIGHT - HUD_PADDING - thickness,
            HUD_PADDING + size, UI_DISPLAY_HEIGHT - HUD_PADDING, color, true);
    drawRect(display_buffer, HUD_PADDING, UI_DISPLAY_HEIGHT - HUD_PADDING - size,
            HUD_PADDING + thickness, UI_DISPLAY_HEIGHT - HUD_PADDING, color, true);

    // Bottom-right corner
    drawRect(display_buffer, UI_DISPLAY_WIDTH - HUD_PADDING - size,
            UI_DISPLAY_HEIGHT - HUD_PADDING - thickness,
            UI_DISPLAY_WIDTH - HUD_PADDING, UI_DISPLAY_HEIGHT - HUD_PADDING,
            color, true);
    drawRect(display_buffer, UI_DISPLAY_WIDTH - HUD_PADDING - thickness,
            UI_DISPLAY_HEIGHT - HUD_PADDING - size,
            UI_DISPLAY_WIDTH - HUD_PADDING, UI_DISPLAY_HEIGHT - HUD_PADDING,
            color, true);
}

void ThermalUIRenderer::drawLockStateIndicator(uint32_t* display_buffer,
                                              const TargetLockTracker& tracker) {
    LockState state = tracker.getLockState();
    const char* state_text = "";
    uint32_t color = UIColors::HUD_PRIMARY;

    switch (state) {
        case LockState::SEARCHING:
            state_text = "SEARCH";
            color = UIColors::HUD_PRIMARY;
            break;
        case LockState::LOCKING:
            state_text = "LOCK >>>";
            color = UIColors::HUD_WARNING;
            break;
        case LockState::LOCKED:
            state_text = "** LOCKED **";
            color = UIColors::RETICLE_LOCKED;
            break;
        case LockState::LOST:
            state_text = "LOST";
            color = UIColors::HUD_CRITICAL;
            break;
        default:
            state_text = "???";
            break;
    }

    // Draw state indicator box
    drawRect(display_buffer, HUD_PADDING + HUD_CORNER_SIZE + 5, HUD_PADDING,
            HUD_PADDING + HUD_CORNER_SIZE + 70, HUD_PADDING + 15,
            color, false);  // Outline only

    // Draw state text (simplified - would need bitmap font for real impl)
    drawText(display_buffer, HUD_PADDING + HUD_CORNER_SIZE + 10, HUD_PADDING + 3,
            state_text, color);
}

void ThermalUIRenderer::drawTemperaturePanel(uint32_t* display_buffer,
                                            const TargetLockTracker& tracker) {
    TargetPoint target = tracker.getTarget();
    LockStatistics stats = tracker.getStatistics(nullptr);

    if (!target.is_valid) {
        return;
    }

    // Panel position (bottom-right)
    uint16_t panel_x = UI_DISPLAY_WIDTH - 100;
    uint16_t panel_y = UI_DISPLAY_HEIGHT - 60;

    // Draw semi-transparent panel background
    uint32_t bg_color = blendColors(UIColors::BLACK, UIColors::BLACK, 0.7f);
    drawRect(display_buffer, panel_x - 5, panel_y - 5,
            UI_DISPLAY_WIDTH - 5, UI_DISPLAY_HEIGHT - 5,
            bg_color, true);

    // Draw border
    drawRect(display_buffer, panel_x - 5, panel_y - 5,
            UI_DISPLAY_WIDTH - 5, UI_DISPLAY_HEIGHT - 5,
            UIColors::HUD_PRIMARY, false);

    // Draw temperature text (would need bitmap font for real impl)
    // This is placeholder - actual implementation needs font rendering
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%.1f C", target.temperature);
    drawText(display_buffer, panel_x, panel_y, temp_str, UIColors::HUD_PRIMARY);

    // Draw confidence
    char conf_str[32];
    snprintf(conf_str, sizeof(conf_str), "Conf: %d%%", (uint8_t)(target.confidence * 100));
    drawText(display_buffer, panel_x, panel_y + 12, conf_str, UIColors::HUD_PRIMARY);
}

void ThermalUIRenderer::drawScanningAnimation(uint32_t* display_buffer) {
    static uint32_t scan_position = 0;
    scan_position = (scan_position + 1) % (UI_DISPLAY_HEIGHT - 10);

    uint16_t y = 10 + scan_position;
    for (uint16_t x = 0; x < UI_DISPLAY_WIDTH; ++x) {
        uint32_t idx = y * UI_DISPLAY_WIDTH + x;
        if (idx < UI_DISPLAY_WIDTH * UI_DISPLAY_HEIGHT) {
            // Brighten the scanning line
            uint32_t color = display_buffer[idx];
            uint8_t r = ((color >> 16) & 0xFF) + 30;
            uint8_t g = ((color >> 8) & 0xFF) + 50;
            uint8_t b = (color & 0xFF) + 30;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            display_buffer[idx] = (r << 16) | (g << 8) | b;
        }
    }
}

void ThermalUIRenderer::drawPixel(uint32_t* display_buffer, uint16_t x,
                                 uint16_t y, uint32_t color) {
    if (x >= UI_DISPLAY_WIDTH || y >= UI_DISPLAY_HEIGHT || display_buffer == nullptr) {
        return;
    }
    display_buffer[y * UI_DISPLAY_WIDTH + x] = color;
}

void ThermalUIRenderer::drawRect(uint32_t* display_buffer, uint16_t x0,
                                uint16_t y0, uint16_t x1, uint16_t y1,
                                uint32_t color, bool filled) {
    if (display_buffer == nullptr) {
        return;
    }

    // Ensure proper ordering
    if (x0 > x1) {
        uint16_t tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (y0 > y1) {
        uint16_t tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    if (filled) {
        // Draw filled rectangle
        for (uint16_t y = y0; y <= y1 && y < UI_DISPLAY_HEIGHT; ++y) {
            for (uint16_t x = x0; x <= x1 && x < UI_DISPLAY_WIDTH; ++x) {
                drawPixel(display_buffer, x, y, color);
            }
        }
    } else {
        // Draw rectangle outline
        for (uint16_t x = x0; x <= x1 && x < UI_DISPLAY_WIDTH; ++x) {
            drawPixel(display_buffer, x, y0, color);
            drawPixel(display_buffer, x, y1, color);
        }
        for (uint16_t y = y0; y <= y1 && y < UI_DISPLAY_HEIGHT; ++y) {
            drawPixel(display_buffer, x0, y, color);
            drawPixel(display_buffer, x1, y, color);
        }
    }
}

uint32_t ThermalUIRenderer::temperatureToThermalColor(float temperature,
                                                     float min_temp,
                                                     float max_temp) {
    float normalized = (temperature - min_temp) / (max_temp - min_temp);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    uint8_t r = 0, g = 0, b = 0;

    // Six-segment thermal color ramp
    if (normalized < 0.167f) {
        float t = normalized / 0.167f;
        b = (uint8_t)(255.0f * t);
    } else if (normalized < 0.333f) {
        float t = (normalized - 0.167f) / 0.167f;
        b = 255;
        g = (uint8_t)(255.0f * t);
    } else if (normalized < 0.5f) {
        float t = (normalized - 0.333f) / 0.167f;
        b = (uint8_t)(255.0f * (1.0f - t));
        g = 255;
    } else if (normalized < 0.667f) {
        float t = (normalized - 0.5f) / 0.167f;
        g = 255;
        r = (uint8_t)(255.0f * t);
    } else if (normalized < 0.833f) {
        float t = (normalized - 0.667f) / 0.167f;
        r = 255;
        g = (uint8_t)(255.0f * (1.0f - t));
    } else {
        float t = (normalized - 0.833f) / 0.167f;
        r = 255;
        g = (uint8_t)(255.0f * t);
        b = (uint8_t)(255.0f * t);
    }

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t ThermalUIRenderer::blendColors(uint32_t fg, uint32_t bg, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    uint8_t fg_r = (fg >> 16) & 0xFF;
    uint8_t fg_g = (fg >> 8) & 0xFF;
    uint8_t fg_b = fg & 0xFF;

    uint8_t bg_r = (bg >> 16) & 0xFF;
    uint8_t bg_g = (bg >> 8) & 0xFF;
    uint8_t bg_b = bg & 0xFF;

    uint8_t out_r = (uint8_t)(fg_r * alpha + bg_r * (1.0f - alpha));
    uint8_t out_g = (uint8_t)(fg_g * alpha + bg_g * (1.0f - alpha));
    uint8_t out_b = (uint8_t)(fg_b * alpha + bg_b * (1.0f - alpha));

    return ((uint32_t)out_r << 16) | ((uint32_t)out_g << 8) | out_b;
}

void ThermalUIRenderer::drawText(uint32_t* display_buffer, uint16_t x,
                                uint16_t y, const char* text, uint32_t color) {
    // Simple placeholder - would need actual font bitmap for real implementation
    // This is a stub that shows where text rendering would go
    (void)display_buffer;
    (void)x;
    (void)y;
    (void)text;
    (void)color;
}
