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
TMouse::TMouse() {
  visible = 0;
  setMousePosition(160, 100);
  leftDown = 0;
  rightDown = 0;
  show();
}

/**
 * @brief Cleanup - hide cursor
 */
TMouse::~TMouse() { hide(); }

/**
 * @brief Check if cursor is within a bounding rectangle
 */
int TMouse::checkInside(int x1, int y1, int x2, int y2) {
  return (x >= x1) && (x <= x2) && (y >= y1) && (y <= y2) ? 1 : 0;
}

/**
 * @brief Conditionally hide cursor if inside rectangle
 *
 * Legacy API for dirty-rectangle cursor management.
 * In this WASM port, cursor is drawn in JavaScript.
 */
void TMouse::hide(int x1, int y1, int x2, int y2) {
  if (visible) {
    visible = 0;
  }
}

/**
 * @brief Show the mouse cursor
 */
void TMouse::show() {
  if (!visible) {
    visible = 1;
  }
}

/**
 * @brief Hide the mouse cursor
 */
void TMouse::hide() {
  if (visible) {
    visible = 0;
  }
}

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
  if (bState == 1) {
    leftDown = 1;
    rightDown = 0;
  } else if (bState == 2) {
    leftDown = 0;
    rightDown = 1;
  } else {
    leftDown = 0;
    rightDown = 0;
  }
}
