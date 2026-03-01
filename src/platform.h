/**
 * @file platform.h
 * @brief Platform abstraction layer for Slime WASM
 *
 * Provides minimal standard library replacements, simulation constants,
 * bounds-checking helpers, and JS import declarations. This file allows
 * the C++ code to compile to WASM without Emscripten or a full libc.
 */

#pragma once

#include <stdint.h>

// =============================================================================
// Minimal type aliases (no libc on wasm32)
// =============================================================================

using size_t = __SIZE_TYPE__;

// =============================================================================
// JS Imports - Functions provided by the JavaScript runtime
// =============================================================================
extern "C" {
/// Generate a random integer in range [0, max)
int random_int(int max);

/// Log an integer value to browser console (for debugging)
void console_log(int val);

/// Get current time in milliseconds
double get_time_ms();

/// Standard math functions (provided by JS Math object)
double sin(double x);
double cos(double x);
double fabs(double x);
}

// =============================================================================
// Simulation Constants
// =============================================================================

// Screen dimensions
constexpr int SCREEN_WIDTH = 320;  ///< Total screen width in pixels
constexpr int SCREEN_HEIGHT = 200; ///< Total screen height in pixels
constexpr int FIELD_WIDTH = 300;   ///< Playable simulation area width
constexpr int FIELD_HEIGHT = 200;  ///< Playable simulation area height
constexpr int SIDEBAR_X = 300;     ///< X position where sidebar starts

// Cell value constants
constexpr int WALL_VALUE = 99;   ///< Field value representing a wall
constexpr int MAX_WATER = 97;    ///< Maximum water density per cell
constexpr int DRAIN_VALUE = 100; ///< Field value representing a drain

// Simulation parameters
constexpr int RAIN_PROBABILITY = 100; ///< 1 in N chance per column per frame
constexpr int WATER_SPAWN_AMOUNT = 5; ///< Initial water value when spawned
constexpr int ERASER_SIZE = 5;        ///< Eraser brush size in pixels
constexpr int WATER_ADD_RADIUS = 4;   ///< Water brush radius
constexpr int DENSITY_FLOW = 2;       ///< Water mass transferred per flow step

// VGA palette color indices
constexpr int DARKGRAY = 8;
constexpr int WHITE = 15;
constexpr int TRANSPARENT_COLOR = 16; ///< Icon bitmap transparency key

// =============================================================================
// Bounds Checking Helpers
// =============================================================================

/**
 * @brief Check if coordinates are within the simulation field
 * @param x X coordinate (0 to FIELD_WIDTH-1)
 * @param y Y coordinate (0 to FIELD_HEIGHT-1)
 * @return true if within bounds
 */
[[nodiscard]] constexpr bool inField(int x, int y) noexcept {
  return x >= 0 && x < FIELD_WIDTH && y >= 0 && y < FIELD_HEIGHT;
}

/**
 * @brief Check if coordinates are within the interior of the simulation field
 * @param x X coordinate (1 to FIELD_WIDTH-2)
 * @param y Y coordinate (1 to FIELD_HEIGHT-2)
 * @return true if within interior bounds (excludes border cells)
 */
[[nodiscard]] constexpr bool inFieldInterior(int x, int y) noexcept {
  return x > 0 && x < FIELD_WIDTH - 1 && y > 0 && y < FIELD_HEIGHT - 1;
}

/**
 * @brief Check if coordinates are within the screen
 * @param x X coordinate (0 to SCREEN_WIDTH-1)
 * @param y Y coordinate (0 to SCREEN_HEIGHT-1)
 * @return true if within bounds
 */
[[nodiscard]] constexpr bool inScreen(int x, int y) noexcept {
  return x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT;
}

// =============================================================================
// Minimal Libc Replacements
// =============================================================================

[[nodiscard]] constexpr int abs(int x) noexcept { return x < 0 ? -x : x; }
[[nodiscard]] constexpr double abs(double x) noexcept { return x < 0 ? -x : x; }

inline void *memset(void *dst, int c, size_t n) noexcept {
  return __builtin_memset(dst, c,
                          n); // emits WASM memory.fill with -mbulk-memory
}

inline void *memcpy(void *dst, const void *src, size_t n) noexcept {
  return __builtin_memcpy(dst, src,
                          n); // emits WASM memory.copy with -mbulk-memory
}

[[nodiscard]] inline size_t strlen(const char *s) noexcept {
  size_t len = 0;
  while (*s++)
    len++;
  return len;
}

inline char *strncpy(char *dst, const char *src, size_t n) noexcept {
  char *d = dst;
  while (n-- && *src)
    *d++ = *src++;
  while (n--)
    *d++ = 0;
  return dst;
}

/// Swap two integers by reference
inline void swap(int &a, int &b) noexcept {
  int tmp = a;
  a = b;
  b = tmp;
}

// =============================================================================
// Graphics Functions (implemented in main.cpp)
// =============================================================================

/// Draw a single pixel at (x, y) with color c
void putpixel(int x, int y, int c);

/// Fill a rectangle with the default bar color
void bar(int x1, int y1, int x2, int y2);

/// Draw a white line between two points
void line(int x1, int y1, int x2, int y2);

/// Draw a colored line (used for UI borders)
void drawline(int x1, int y1, int x2, int y2, int col);
