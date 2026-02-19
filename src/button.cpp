/**
 * @file button.cpp
 * @brief UI Button implementation
 */

#include "button.h"
#include "mouse.h"
#include "platform.h"

extern TMouse mouse;

/**
 * @brief Construct a new TButton with default white icon
 */
TButton::TButton() {
  for (int x = 0; x < 16; x++) {
    for (int y = 0; y < 16; y++) {
      blitMap[x][y] = 1; // Default to blue color
    }
  }
}

/**
 * @brief Copy a 16x16 icon into the button's blitMap
 */
void TButton::setBlitMap(void *map) { memcpy(blitMap, map, sizeof(blitMap)); }

/**
 * @brief Render the button with 3D border effect and icon
 *
 * The button draws with a beveled edge effect:
 * - When up: top/left edges are light, bottom/right are dark
 * - When down: colors are swapped for pressed appearance
 */
void TButton::paint() {
  mouse.hide();

  // Draw 3D border effect
  if (isDown == 0) {
    // Button is up - light top/left, dark bottom/right
    drawline(x1, y1, x2, y1, DARKGRAY);
    drawline(x1, y1, x1, y2, DARKGRAY);
    drawline(x2, y2, x2, y1, 0);
    drawline(x2, y2, x1, y2, 0);
  } else {
    // Button is down - dark top/left, light bottom/right
    drawline(x1, y1, x2, y1, 0);
    drawline(x1, y1, x1, y2, 0);
    drawline(x2, y2, x2, y1, DARKGRAY);
    drawline(x2, y2, x1, y2, DARKGRAY);
  }

  // Blit the icon (color 16 = transparent)
  for (int x = 0; x < 16; x++) {
    for (int y = 0; y < 16; y++) {
      if (blitMap[x][y] != 16) {
        putpixel(x1 + 1 + x, y1 + 1 + y, blitMap[x][y]);
      }
    }
  }

  mouse.show();
}
