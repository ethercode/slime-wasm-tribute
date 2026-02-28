/**
 * @file mouse.cpp
 * @brief Mouse input implementation
 *
 * Reads mouse state from JavaScript-provided global variables
 * and manages cursor visibility.
 */

#include "mouse.h"

// External input state variables (set by JavaScript runtime)
extern int input_mouse_x;   ///< Current mouse X position
extern int input_mouse_y;   ///< Current mouse Y position
extern int input_mouse_btn; ///< Button state: 0=none, 1=left, 2=right

/**
 * @brief Initialize mouse at screen center
 */
TMouse::TMouse()
    : x(160), y(100), visible(false), leftDown(false), rightDown(false),
      oldLeftDown(false), oldRightDown(false) {
  show();
}

/**
 * @brief Cleanup - hide cursor
 */
TMouse::~TMouse() { hide(); }

/**
 * @brief Check if cursor is within a bounding rectangle
 */
bool TMouse::checkInside(int x1, int y1, int x2, int y2) const {
  return (x >= x1) && (x <= x2) && (y >= y1) && (y <= y2);
}

/**
 * @brief Show the mouse cursor
 */
void TMouse::show() { visible = true; }

/**
 * @brief Hide the mouse cursor
 */
void TMouse::hide() { visible = false; }

/**
 * @brief Directly set cursor position
 */
void TMouse::setMousePosition(int newX, int newY) {
  x = newX;
  y = newY;
}

/**
 * @brief Update mouse state from JavaScript input
 *
 * Called once per frame to sync with browser input.
 * Preserves previous button states for click detection.
 *
 * Button state encoding from JS:
 * - 0: No buttons pressed
 * - 1: Left button pressed
 * - 2: Right button pressed
 */
void TMouse::update() {
  // Read from global input state
  x = input_mouse_x;
  y = input_mouse_y;
  int bState = input_mouse_btn;

  // Preserve previous state for edge detection
  oldLeftDown = leftDown;
  oldRightDown = rightDown;

  // Decode button state (mutually exclusive in this implementation)
  leftDown = (bState == 1);
  rightDown = (bState == 2);
}
