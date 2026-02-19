/**
 * @file platform.h
 * @brief Platform abstraction layer for Slime WASM
 *
 * Provides minimal standard library replacements, simulation constants,
 * bounds-checking helpers, and JS import declarations. This file allows
 * the C++ code to compile to WASM without Emscripten or a full libc.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#define NULL 0

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

// =============================================================================
// Bounds Checking Helpers
// =============================================================================

/**
 * @brief Check if coordinates are within the simulation field
 * @param x X coordinate (0-299)
 * @param y Y coordinate (0-199)
 * @return true if within bounds
 */
inline bool inField(int x, int y) {
  return x >= 0 && x < FIELD_WIDTH && y >= 0 && y < FIELD_HEIGHT;
}

/**
 * @brief Check if coordinates are within the screen
 * @param x X coordinate (0-319)
 * @param y Y coordinate (0-199)
 * @return true if within bounds
 */
inline bool inScreen(int x, int y) {
  return x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT;
}

// =============================================================================
// Minimal Libc Replacements
// =============================================================================

inline int abs(int x) { return x < 0 ? -x : x; }
inline double abs(double x) { return x < 0 ? -x : x; }

inline void *memset(void *dst, int c, unsigned long n) {
  uint8_t *d = (uint8_t *)dst;
  while (n--)
    *d++ = c;
  return dst;
}

inline void *memcpy(void *dst, const void *src, unsigned long n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (n--)
    *d++ = *s++;
  return dst;
}

inline unsigned long strlen(const char *s) {
  unsigned long len = 0;
  while (*s++)
    len++;
  return len;
}

inline char *strncpy(char *dst, const char *src, unsigned long n) {
  char *d = dst;
  while (n-- && *src)
    *d++ = *src++;
  while (n--)
    *d++ = 0;
  return dst;
}

// =============================================================================
// Graphics Functions (implemented in main.cpp)
// =============================================================================

/// Draw a single pixel at (x, y) with color c
void putpixel(int x, int y, int c);

/// Fill a rectangle with the default bar color
void bar(int x1, int y1, int x2, int y2);

/// Draw a line between two points
void line(int x1, int y1, int x2, int y2);

/// Draw a colored line (used for UI borders)
void drawline(float zx1, float zy1, float zx2, float zy2, int col);

/// Swap two integer values
void swap(int *a, int *b);

#endif
