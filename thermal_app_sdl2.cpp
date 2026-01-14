#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <SDL2/SDL.h>
#include "MLX90640Driver.h"
#include "MLX90640Upscaler.h"
#include "TargetLockTracker.h"
#include "ThermalUIRenderer.h"

// Standard C++ replacements for Arduino functions
namespace Arduino {
    using Clock = std::chrono::steady_clock;
    static auto startup_time = Clock::now();

    inline unsigned long millis() {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startup_time);
        return static_cast<unsigned long>(duration.count());
    }

    inline unsigned long micros() {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startup_time);
        return static_cast<unsigned long>(duration.count());
    }

    inline void delay(unsigned long ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    class SerialClass {
    public:
        void begin(unsigned long baud) { /* No-op in desktop */ }
        
        template<typename T>
        SerialClass& print(const T& value) {
            std::cout << value;
            std::cout.flush();
            return *this;
        }

        SerialClass& print(const char* str) {
            std::cout << str;
            std::cout.flush();
            return *this;
        }

        SerialClass& print(float value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value;
            std::cout.flush();
            return *this;
        }

        SerialClass& print(double value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value;
            std::cout.flush();
            return *this;
        }

        template<typename T>
        SerialClass& println(const T& value) {
            std::cout << value << std::endl;
            return *this;
        }

        SerialClass& println(const char* str) {
            std::cout << str << std::endl;
            return *this;
        }

        SerialClass& println(float value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value << std::endl;
            return *this;
        }

        SerialClass& println(double value, int precision) {
            std::cout << std::fixed << std::setprecision(precision) << value << std::endl;
            return *this;
        }

        SerialClass& println() {
            std::cout << std::endl;
            return *this;
        }

        bool available() { return false; }
        char read() { return '\0'; }
    };

    static SerialClass Serial;
}

using Arduino::Serial;
using Arduino::millis;
using Arduino::micros;
using Arduino::delay;

// ============================================================================
// Configuration
// ============================================================================

#define SDA_PIN 21
#define SCL_PIN 22

// Display dimensions
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Window scale factor for desktop visibility
#define WINDOW_SCALE 2  // 640x480 window

// Static system instances
static MLX90640Driver thermal_sensor;
static MLX90640Upscaler upscaler;
static TargetLockTracker target_tracker;
static ThermalUIRenderer ui_renderer;

// Data buffers
static float raw_frame[MLX90640_PIXEL_COUNT];         // 768 pixels (32x24)
static float upscaled_frame[DISPLAY_WIDTH * DISPLAY_HEIGHT]; // 76,800 pixels (320x240)
static uint32_t display_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT]; // Display framebuffer

// ============================================================================
// SDL2 Window Management
// ============================================================================

class ThermalWindow {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    uint32_t* pixel_buffer;
    int width, height;
    int scale;

public:
    ThermalWindow(int w, int h, int scale_factor) 
        : width(w), height(h), scale(scale_factor), window(nullptr), 
          renderer(nullptr), texture(nullptr), pixel_buffer(nullptr) {
        
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
            return;
        }

        window = SDL_CreateWindow(
            "Thermal Imaging System - Target Lock",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width * scale,
            height * scale,
            SDL_WINDOW_SHOWN
        );

        if (!window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        // Create texture for display buffer
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGB888,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height
        );

        if (!texture) {
            std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        pixel_buffer = new uint32_t[width * height];
        std::cout << "✓ SDL2 window created: " << (width * scale) << "x" << (height * scale) << std::endl;
    }

    ~ThermalWindow() {
        if (pixel_buffer) delete[] pixel_buffer;
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool isValid() const {
        return window && renderer && texture && pixel_buffer;
    }

    void updateDisplay(const uint32_t* frame_data) {
        if (!isValid()) return;

        // Convert frame to display format and scale
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                pixel_buffer[y * width + x] = frame_data[y * width + x];
            }
        }

        // Update texture
        SDL_UpdateTexture(texture, nullptr, pixel_buffer, width * sizeof(uint32_t));

        // Clear and render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Render scaled texture
        SDL_Rect dest_rect = {0, 0, width * scale, height * scale};
        SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);

        SDL_RenderPresent(renderer);
    }

    bool handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    return false;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        return false;
                    }
                    break;
            }
        }
        return true;
    }
};

// ============================================================================
// Simulation
// ============================================================================

void generateSimulatedThermalData(float* frame) {
    // Fill with background temperature (~20°C)
    for (int i = 0; i < MLX90640_PIXEL_COUNT; ++i) {
        frame[i] = 20.0f + (rand() % 50) * 0.1f;
    }
    
    // Add hot spot in center (200°C)
    int center_idx = (12 * 32) + 16;
    frame[center_idx] = 200.0f;
    
    // Add thermal gradient around hotspot
    for (int y = 10; y < 14; ++y) {
        for (int x = 14; x < 18; ++x) {
            int idx = (y * 32) + x;
            float dist = sqrtf((x - 16)*(x - 16) + (y - 12)*(y - 12));
            frame[idx] = 200.0f - (dist * 30.0f);
        }
    }
}

uint32_t temperatureToRGB888(float temperature) {
    float min_temp = -20.0f;
    float max_temp = 300.0f;
    float normalized = (temperature - min_temp) / (max_temp - min_temp);

    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    uint8_t r = 0, g = 0, b = 0;

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

bool processThermalFrame() {
    // Step 1: Generate simulated thermal data
    generateSimulatedThermalData(raw_frame);

    // Step 2: Upscale to display resolution
    bool upscale_ok = upscaler.upscale(raw_frame, upscaled_frame);
    if (!upscale_ok) {
        Serial.println("Upscaling failed!");
        return false;
    }

    // Step 3: Process thermal frame and update target lock
    TargetPoint target = target_tracker.processThermalFrame(upscaled_frame);

    // Step 4: Convert thermal data to RGB display colors
    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
        float temp = upscaled_frame[i];
        display_buffer[i] = temperatureToRGB888(temp);
    }

    // Step 5: Draw targeting reticle
    if (target.is_valid) {
        target_tracker.drawReticle((uint32_t*)display_buffer);
    }

    // Step 6: Render UI overlay
    ui_renderer.renderHUDOverlay(display_buffer, target_tracker);

    return true;
}

// ============================================================================
// Main Application
// ============================================================================

int main() {
    Serial.begin(115200);
    
    // Initialize system
    Serial.println("\n=====================================");
    Serial.println("Thermal Imaging Desktop Application");
    Serial.println("=====================================\n");

    // Initialize components
    upscaler.setInterpolationMethod(InterpolationMethod::BILINEAR);
    target_tracker.setReticleStyle(ReticleStyle::CORNER_BRACKETS);
    target_tracker.setHysteresisThreshold(0.5f);
    target_tracker.setSmoothingFactor(0.7f);
    
    Serial.println("✓ Thermal system initialized");
    Serial.println("✓ Upscaler: Bilinear interpolation");
    Serial.println("✓ Tracker: Corner bracket reticle");
    Serial.println("✓ UI: Military HUD overlay\n");

    // Create SDL2 window
    ThermalWindow window(DISPLAY_WIDTH, DISPLAY_HEIGHT, WINDOW_SCALE);
    if (!window.isValid()) {
        Serial.println("ERROR: Failed to create SDL2 window!");
        return 1;
    }

    Serial.println("Controls:");
    Serial.println("  ESC or close window to exit\n");

    Serial.println("========== THERMAL DISPLAY ACTIVE ==========\n");

    // Main event loop
    unsigned long frame_count = 0;
    unsigned long start_time = millis();
    bool running = true;

    while (running) {
        unsigned long frame_start = millis();

        // Process thermal frame
        if (processThermalFrame()) {
            // Update display
            window.updateDisplay(display_buffer);
            frame_count++;

            // Print status every 64 frames (4 seconds at 16 Hz)
            if (frame_count % 64 == 0) {
                unsigned long elapsed = millis() - start_time;
                float fps = (frame_count * 1000.0f) / elapsed;
                Serial.print("Frame ");
                Serial.print(frame_count);
                Serial.print(" | FPS: ");
                Serial.print(fps, 1);
                Serial.print(" | Lock: ");
                
                LockState state = target_tracker.getLockState();
                switch (state) {
                    case LockState::SEARCHING:
                        Serial.println("SEARCHING");
                        break;
                    case LockState::LOCKING:
                        Serial.println("LOCKING");
                        break;
                    case LockState::LOCKED:
                        Serial.println("LOCKED");
                        break;
                    case LockState::LOST:
                        Serial.println("LOST");
                        break;
                }
            }
        }

        // Handle window events
        running = window.handleEvents();

        // Frame-rate limiting (63ms per frame for 16 Hz)
        unsigned long frame_time = millis() - frame_start;
        if (frame_time < 63) {
            delay(63 - frame_time);
        }
    }

    // Cleanup
    unsigned long total_time = millis() - start_time;
    Serial.println("\n========== APPLICATION CLOSED ==========");
    Serial.print("Total frames: ");
    Serial.println(frame_count);
    Serial.print("Total time: ");
    Serial.print(total_time / 1000.0f, 1);
    Serial.println(" seconds");
    Serial.print("Average FPS: ");
    Serial.println((frame_count * 1000.0f) / total_time, 1);

    return 0;
}
