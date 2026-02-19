/**
 * @file mouse.h
 * @brief Mouse input handling
 *
 * Provides mouse state tracking including position and button states.
 * Reads from global input variables set by JavaScript.
 */

#ifndef MOUSE_H
#define MOUSE_H

#include "platform.h"

/**
 * @class TMouse
 * @brief Manages mouse cursor state and input
 *
 * Tracks current and previous button states to detect clicks.
 * The cursor position and button state are updated each frame
 * from JavaScript-provided global variables.
 */
class TMouse {
public:
  TMouse();
  ~TMouse();

  /**
   * @brief Update mouse state from global input variables
   *
   * Should be called once per frame. Updates position and button
   * states, preserving previous state for edge detection.
   */
  void update();

  /// Show the mouse cursor
  void show();

  /// Hide the mouse cursor
  void hide();

  /**
   * @brief Set cursor position directly
   * @param newX X coordinate
   * @param newY Y coordinate
   */
  void setMousePosition(int newX, int newY);

  /**
   * @brief Check if cursor is inside a rectangle
   * @param x1 Left edge
   * @param y1 Top edge
   * @param x2 Right edge
   * @param y2 Bottom edge
   * @return 1 if inside, 0 if outside
   */
  int checkInside(int x1, int y1, int x2, int y2);

  /**
   * @brief Hide cursor if inside rectangle (legacy API)
   */
  void hide(int x1, int y1, int x2, int y2);

  // Current cursor position
  int x = 0;
  int y = 0;

  int visible = 0;      ///< Cursor visibility flag
  int leftDown = 0;     ///< Current left button state
  int rightDown = 0;    ///< Current right button state
  int oldLeftDown = 0;  ///< Previous frame left button state
  int oldRightDown = 0; ///< Previous frame right button state
};

#endif
