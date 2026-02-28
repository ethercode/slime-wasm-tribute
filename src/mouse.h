/**
 * @file mouse.h
 * @brief Mouse input handling
 *
 * Provides mouse state tracking including position and button states.
 * Reads from global input variables set by JavaScript.
 */

#pragma once

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
   * @return true if inside, false if outside
   */
  [[nodiscard]] bool checkInside(int x1, int y1, int x2, int y2) const;

  // Current cursor position
  int x = 0;
  int y = 0;

  bool visible = false;      ///< Cursor visibility flag
  bool leftDown = false;     ///< Current left button state
  bool rightDown = false;    ///< Current right button state
  bool oldLeftDown = false;  ///< Previous frame left button state
  bool oldRightDown = false; ///< Previous frame right button state
};
