/**
 * @file button.h
 * @brief UI Button component for the sidebar
 *
 * Provides a simple button class with icon support, toggle state,
 * and 3D-style pressed/unpressed rendering.
 */

#pragma once

#include "platform.h"

/**
 * @class TButton
 * @brief Represents a clickable UI button with an icon
 *
 * Buttons are used in the sidebar for tool selection and actions.
 * Each button has a 16x16 pixel icon (blitMap) and can be either
 * a toggle button or a momentary button.
 */
class TButton {
public:
  TButton();

  /**
   * @brief Set the button's icon from a 16x16 pixel map
   * @param map Reference to a 16x16 array of color indices
   *            Color TRANSPARENT_COLOR (16) is treated as transparent
   */
  void setBlitMap(const uint8_t (&map)[16][16]);

  /**
   * @brief Render the button to the screen
   *
   * Draws the button border (3D effect based on isDown state)
   * and blits the icon on top.
   */
  void paint();

  // Button rectangle (screen coordinates)
  int x1 = 0, y1 = 0, x2 = 0, y2 = 0;

  int tag = 1; ///< Identifier for button actions (Tool/Action enum value)
  bool isDown = false;    ///< Current pressed state
  bool isToggler = false; ///< If true, button toggles state on click

private:
  uint8_t blitMap[16][16]; ///< 16x16 icon bitmap (color indices)
};
