/**
 * @file main.cpp
 * @brief Slime WASM - Water Physics Simulation
 *
 * A WebAssembly port of the classic DOS "Slime" water simulation.
 * This file contains the main game loop, rendering, physics simulation,
 * and UI handling.
 *
 * The simulation uses a cellular automata approach where each cell
 * contains a water density value (0-97) or wall marker (99).
 * Water flows to neighboring cells based on density gradients.
 *
 * @author Original DOS version: Blaine Murray (http://ethercode.net/)
 *         https://github.com/ethercode/slime-dos
 * @author WASM port: Ewald Horn
 */

#include "button.h"
#include "mouse.h"
#include "platform.h"

// Needed for static object destruction with -nostdlib
extern "C" void *__dso_handle = nullptr;
extern "C" int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle) {
  return 0;
}

// =============================================================================
// Global State
// =============================================================================

/// RGBA video buffer - written by C++, read by JavaScript for canvas rendering
uint8_t video_buffer[SCREEN_WIDTH * SCREEN_HEIGHT * 4];

/// Simulation field: each cell is water density (0-97), wall (99), or drain
/// (100)
uint8_t field[SCREEN_WIDTH][SCREEN_HEIGHT];

/// Mouse input from JavaScript
int input_mouse_x = 0;
int input_mouse_y = 0;
int input_mouse_btn = 0;

/// UI buttons (10 slots, not all used)
TButton buttons[10];

/// Global mouse state
TMouse mouse;

// =============================================================================
// Enums for Type-Safe Constants
// =============================================================================

/// Tools available in the sidebar (indices 1-5)
enum class Tool {
  Pencil = 1,      ///< Draw walls
  EraserWall = 2,  ///< Erase walls
  EraserWater = 3, ///< Erase water
  Line = 4,        ///< Line drawing mode
  Free = 5         ///< Freehand drawing mode
};

/// Actions triggered by sidebar buttons
enum class Action {
  Reset = 10,      ///< Reset simulation
  Pause = 11,      ///< Pause/resume
  ClearLines = 12, ///< Clear all walls
  ClearWater = 13, ///< Clear all water
  Rain = 22        ///< Toggle rain mode
};

/// Current eraser tool mode
enum class EraserMode { None = 0, Wall = 1, Water = 2 };

// =============================================================================
// State Structs
// =============================================================================

/**
 * @brief Global game state variables
 */
struct GameState {
  EraserMode eraser = EraserMode::None; ///< Current eraser mode
  int drawmode = 1;                     ///< 1 = Line mode, 2 = Freehand mode
  bool rainmode = false;                ///< Rain enabled
  bool paused = false;                  ///< Simulation paused
  int frames = 0;                       ///< Frame counter
};

/**
 * @brief Input state for drawing operations
 */
struct InputState {
  bool hasLeft = false; ///< Left button was pressed (tracking drag start)
  bool mayDraw = true;  ///< Drawing is allowed
  int x1 = 0, y1 = 0;   ///< Drag start position
};

// --- Global Instances ---
GameState game;
InputState input;

// --- Graphics Implementation ---

// Note: ABGR packing for little-endian WASM commonly works best with canvas
// ImageData

void putpixel(int x, int y, int c) {
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT)
    return;

  int index = (y * SCREEN_WIDTH + x) * 4;

  uint8_t r = 0, g = 0, b = 0;
  // Basic VGA palette mapping
  switch (c % 16) {
  case 0:
    r = 0;
    g = 0;
    b = 0;
    break; // Black
  case 1:
    r = 0;
    g = 0;
    b = 170;
    break; // Blue
  case 2:
    r = 0;
    g = 170;
    b = 0;
    break; // Green
  case 3:
    r = 0;
    g = 170;
    b = 170;
    break; // Cyan
  case 4:
    r = 170;
    g = 0;
    b = 0;
    break; // Red
  case 5:
    r = 170;
    g = 0;
    b = 170;
    break; // Magenta
  case 6:
    r = 170;
    g = 85;
    b = 0;
    break; // Brown
  case 7:
    r = 170;
    g = 170;
    b = 170;
    break; // Light Gray
  case 8:
    r = 85;
    g = 85;
    b = 85;
    break; // Dark Gray
  case 9:
    r = 85;
    g = 85;
    b = 255;
    break; // Light Blue
  case 10:
    r = 85;
    g = 255;
    b = 85;
    break; // Light Green
  case 11:
    r = 85;
    g = 255;
    b = 255;
    break; // Light Cyan
  case 12:
    r = 255;
    g = 85;
    b = 85;
    break; // Light Red
  case 13:
    r = 255;
    g = 85;
    b = 255;
    break; // Light Magenta
  case 14:
    r = 255;
    g = 255;
    b = 85;
    break; // Yellow
  case 15:
    r = 255;
    g = 255;
    b = 255;
    break; // White
  default:
    break;
  }

  // Override for water simulation colors
  // From original screenshot: Blue (low density/surface) → Purple → Red →
  // Yellow (high density/bottom) Water formula: (field+1)/2 + 103 → colors
  // 103-152 for field values 1-97 Lower density (surface) = bluer, Higher
  // density (bottom/accumulated) = redder/yellower
  if (c >= 100) {
    int val = c - 100; // 0-55 range, higher = higher density (bottom of pool)
    if (val < 0)
      val = 0;
    if (val > 55)
      val = 55;

    // Gradient: Blue (low density/surface) → Magenta → Red → Yellow (high
    // density/deep) val 0-14:  Blue → Magenta (B=255, R increases, G=0) val
    // 14-28: Magenta → Red (R=255, B decreases, G=0) val 28-42: Red → Orange
    // (R=255, B=0, G increases) val 42-55: Orange → Yellow (R=255, G increases
    // to 255, B=0)

    if (val < 14) {
      // Blue to Magenta (surface water)
      r = (val * 255) / 14;
      g = 0;
      b = 255;
    } else if (val < 28) {
      // Magenta to Red (mid-depth)
      r = 255;
      g = 0;
      b = 255 - ((val - 14) * 255) / 14;
    } else if (val < 42) {
      // Red to Orange (deeper)
      r = 255;
      g = (val - 28) * 170 / 14; // Green goes 0→170
      b = 0;
    } else {
      // Orange to Yellow (high pressure at bottom)
      r = 255;
      g = 170 + ((val - 42) * 85) / 13; // Green goes 170→255
      if (g > 255)
        g = 255;
      b = 0;
    }
  }

  video_buffer[index] = r;
  video_buffer[index + 1] = g;
  video_buffer[index + 2] = b;
  video_buffer[index + 3] = 255; // Alpha
}

void swap(int *a, int *b) {
  int v = *a;
  *a = *b;
  *b = v;
}

void drawline(float zx1, float zy1, float zx2, float zy2, int col) {
  int x1, y1, x2, y2;
  if (zx1 <= zx2) {
    x1 = zx1;
    y1 = zy1;
    x2 = zx2;
    y2 = zy2;
  } else {
    x2 = zx1;
    y2 = zy1;
    x1 = zx2;
    y1 = zy2;
  }
  if ((x1 == x2) && (y1 == y2))
    return;
  float dx = x2 - x1;
  float dy = y2 - y1;
  if (abs(dx) > abs(dy)) {
    float iy = dy / dx;
    float cy = y1;
    if (x1 > x2)
      swap(&x1, &x2);
    for (int x = x1; x <= x2; x++) {
      cy += iy;
      putpixel(x, int(cy), col);
    }
  } else {
    float ix = abs(dx / dy);
    float cx = x1;
    if (y1 > y2) {
      swap(&y1, &y2);
      ix = -ix;
      cx = x2;
    }
    for (int y = y1; y <= y2; y++) {
      cx += ix;
      putpixel(cx, y, col);
    }
  }
}

void line(int x1, int y1, int x2, int y2) { // Stub meant for logic, not drawing
  drawline(x1, y1, x2, y2, WHITE);
}

// Overload for game logic line drawing (modifying 'field') vs visual drawing is
// needed. The original had `line(..., int st)` where st=1 meant update field.
// We need to support that.
void logic_line(float zx1, float zy1, float zx2, float zy2, int st) {
  int x1, y1, x2, y2;
  if (zx1 <= zx2) {
    x1 = zx1;
    y1 = zy1;
    x2 = zx2;
    y2 = zy2;
  } else {
    x2 = zx1;
    y2 = zy1;
    x1 = zx2;
    y1 = zy2;
  }

  if ((x1 == x2) && (y1 == y2))
    return;
  float dx = x2 - x1;
  float dy = y2 - y1;

  if (abs(dx) > abs(dy)) {
    float iy = dy / dx;
    float cy = y1;
    if (x1 > x2)
      swap(&x1, &x2);
    for (int x = x1; x <= x2; x++) {
      cy += iy;
      if (x >= 0 && x < 300 && (int)cy >= 0 && (int)cy < 200) {
        if (st == 1)
          field[x][(int)cy] = 99;
        else
          putpixel(x, (int)cy, st);
      }
    }
  } else {
    float ix = abs(dx / dy);
    float cx = x1;
    if (y1 > y2) {
      swap(&y1, &y2);
      ix = -ix;
      cx = x2;
    }
    for (int y = y1; y <= y2; y++) {
      cx += ix;
      if ((int)cx >= 0 && (int)cx < 300 && y >= 0 && y < 200) {
        if (st == 1)
          field[(int)cx][y] = 99;
        else
          putpixel((int)cx, y, st);
      }
    }
  }
}

void bar(int x1, int y1, int x2, int y2) {
  for (int x = x1; x <= x2; x++) {
    for (int y = y1; y <= y2; y++) {
      if (x >= 0 && x < 320 && y >= 0 && y < 200) {
        putpixel(x, y, 7);
      }
    }
  }
}

// --- Game Logic Functions ---

void n(void) {
  for (int x = 0; x < 320; x++) {
    for (int y = 0; y < 200; y++)
      field[x][y] = 0;
  }
  // Borders
  for (int x = 0; x < 300; x++) {
    field[x][0] = 99;
    field[x][199] = 99;
  }
  for (int y = 0; y < 200; y++) {
    field[0][y] = 99;
    field[299][y] = 99;
  }
  game.rainmode = false;
}

void clearLines() {
  for (int x = 1; x < 299; x++) {
    for (int y = 1; y < 199; y++) {
      if (field[x][y] == 99)
        field[x][y] = 0;
    }
  }
}

void clearWater() {
  for (int x = 0; x < 300; x++) {
    for (int y = 0; y < 200; y++) {
      if (field[x][y] < 99)
        field[x][y] = 0;
    }
  }
  // Re-add borders
  for (int x = 0; x < 300; x++) {
    field[x][0] = 99;
    field[x][199] = 99;
  }
  for (int y = 0; y < 200; y++) {
    field[0][y] = 99;
    field[299][y] = 99;
  }
  game.rainmode = false;
}

void addWater() {
  int t = 4;
  int mmx = mouse.x;
  int mmy = mouse.y;
  for (int qx = -t; qx <= t; qx++) {
    for (int qy = -t * 2; qy < 0; qy++) {
      if ((qy + mmy) >= 0) {
        if (qx + mmx < 319 && qx + mmx > 0 && qy + mmy > 0 && qy + mmy < 200) {
          if (field[qx + mmx][qy + mmy] < 99) {
            field[qx + mmx][qy + mmy] = random_int(5);
          }
        }
      }
    }
  }
}

void killWall() {
  int sx = mouse.x - 2;
  int sy = mouse.y - 2;
  for (int x = sx; x < sx + 5; x++) {
    for (int y = sy; y < sy + 5; y++) {
      if (x > 0 && x < 299 && y > 0 && y < 199 && field[x][y] == 99) {
        field[x][y] = 0;
      }
    }
  }
}

void killWater() {
  int sx = mouse.x - 2;
  int sy = mouse.y - 2;
  for (int x = sx; x < sx + 5; x++) {
    for (int y = sy; y < sy + 5; y++) {
      if (x > 0 && x < 300 && y > 0 && y < 199 && field[x][y] < 99) {
        field[x][y] = 0;
      }
    }
  }
}

void check() {
  for (int a = 0; a < 10; a++) {
    TButton &btn = buttons[a];

    // This is a simplified port of check()
    if (mouse.leftDown == 1 && mouse.oldLeftDown == 0) {
      if (mouse.checkInside(btn.x1, btn.y1, btn.x2, btn.y2)) {
        // Logic for tool switching
        // Standard tools map directly to indices 1-5
        if (a == (int)Tool::Pencil) {
          buttons[(int)Tool::Pencil].isDown = 1;
          buttons[(int)Tool::EraserWall].isDown = 0;
          buttons[(int)Tool::EraserWater].isDown = 0;
          game.eraser = EraserMode::None;
        }
        if (a == (int)Tool::EraserWall) {
          buttons[(int)Tool::Pencil].isDown = 0;
          buttons[(int)Tool::EraserWall].isDown = 1;
          buttons[(int)Tool::EraserWater].isDown = 0;
          game.eraser = EraserMode::Wall;
        }
        if (a == (int)Tool::EraserWater) {
          buttons[(int)Tool::Pencil].isDown = 0;
          buttons[(int)Tool::EraserWall].isDown = 0;
          buttons[(int)Tool::EraserWater].isDown = 1;
          game.eraser = EraserMode::Water;
        }
        if (a == (int)Tool::Line) {
          buttons[(int)Tool::Line].isDown = 1;
          buttons[(int)Tool::Free].isDown = 0;
          game.drawmode = 1;
        }
        if (a == (int)Tool::Free) {
          buttons[(int)Tool::Line].isDown = 0;
          buttons[(int)Tool::Free].isDown = 1;
          game.drawmode = 2;
        }
      }
    }

    // Toggle checking
    if (mouse.checkInside(btn.x1, btn.y1, btn.x2, btn.y2)) {
      if (mouse.leftDown == 0 && mouse.oldLeftDown == 1 && btn.isToggler == 1) {
        if (btn.tag == (int)Action::Reset)
          n(); // Reset
        if (btn.tag == (int)Action::Pause)
          game.paused = !game.paused;
        if (btn.tag == (int)Action::ClearLines)
          clearLines();
        if (btn.tag == (int)Action::ClearWater)
          clearWater();
        if (btn.tag == (int)Action::Rain)
          game.rainmode = !game.rainmode;

        btn.isDown = 0; // Flash effect
      }
    }
    btn.paint();
  }
}

void drawUI() {
  // Re-create buttons if first run
  static int initialized = 0;
  if (!initialized) {
    bar(300, 0, 319, 199);

    // No dynamic allocation needed!
    // Just setting values.

    // --- Button Definitions ---
    // Helper lambda for setup?
    auto setupBtn = [&](int i, int y1, int y2, int tag) {
      buttons[i].x1 = 301;
      buttons[i].y1 = y1;
      buttons[i].x2 = 318;
      buttons[i].y2 = y2;
      buttons[i].tag = tag;
    };

    setupBtn(0, 2, 19, (int)Action::Rain);
    setupBtn(1, 27, 44, 0);   // Pencil
    setupBtn(2, 47, 64, 0);   // Eraser Wall
    setupBtn(3, 67, 84, 0);   // Eraser Water
    setupBtn(4, 92, 109, 0);  // Line
    setupBtn(5, 112, 129, 0); // Free
    setupBtn(6, 137, 154, (int)Action::Pause);

    // Small Buttons
    setupBtn(8, 158, 166, (int)Action::ClearLines);
    setupBtn(9, 169, 177, (int)Action::ClearWater);

    setupBtn(7, 181, 198, (int)Action::Reset);

    // Initial States
    buttons[(int)Tool::Pencil].isDown = 1;
    buttons[(int)Tool::Line].isDown = 1;

    for (auto &btn : buttons) {
      if (btn.tag >= 10)
        btn.isToggler = 1;
    }

    // --- Icon Generation ---
    uint8_t icon[16][16];

    // Helper to fill icon
    auto fillIcon = [&](int color) {
      for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
          icon[x][y] = color;
    };

    // 0: Rain (Noise)
    fillIcon(0);
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++)
        if (random_int(5) == 1)
          icon[x][y] = 1;
    buttons[0].setBlitMap(&icon);

    // 1: Pencil (Solid Blue)
    fillIcon(1);
    buttons[1].setBlitMap(&icon);

    // 2: Eraser Wall (Gray box)
    fillIcon(0);
    for (int y = 3; y < 13; y++)
      for (int x = 3; x < 13; x++)
        icon[x][y] = 15; // White center
    buttons[2].setBlitMap(&icon);

    // 3: Eraser Water (Blue box)
    fillIcon(1); // Blue background
    for (int y = 3; y < 13; y++)
      for (int x = 3; x < 13; x++)
        icon[x][y] = 11; // Light Blue center
    buttons[3].setBlitMap(&icon);

    // 4: Line (Diagonal)
    fillIcon(0);
    for (int i = 0; i < 16; i++)
      icon[i][i] = 15;
    buttons[4].setBlitMap(&icon);

    // 5: Free (Curve-ish)
    fillIcon(0);
    for (int i = 0; i < 16; i++)
      icon[i][(i * i) / 16] = 15; // Simple parabola
    buttons[5].setBlitMap(&icon);

    // 6: Pause (Two bars)
    fillIcon(0);
    for (int y = 2; y < 14; y++) {
      icon[5][y] = 10;
      icon[6][y] = 10;
      icon[9][y] = 10;
      icon[10][y] = 10;
    }
    buttons[6].setBlitMap(&icon);

    // 8: Clear Lines (Small Noise) - MUST be < 8px tall to avoid overlap
    fillIcon(16); // Transparent background
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 16; x++)
        icon[x][y] = (random_int(2) ? 15 : 0);
    buttons[8].setBlitMap(&icon);

    // 9: Clear Water (Small Noise) - MUST be < 8px tall
    fillIcon(16); // Transparent background
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 16; x++)
        icon[x][y] = (random_int(2) ? 1 : 0);
    buttons[9].setBlitMap(&icon);

    // 7: Reset (Cross)
    fillIcon(0);
    for (int i = 3; i < 13; i++) {
      icon[i][i] = 4;
      icon[i][15 - i] = 4;
    }
    buttons[7].setBlitMap(&icon);

    initialized = 1;
  }

  // Repaint all
  bar(300, 0, 319, 199);
  for (auto &btn : buttons)
    btn.paint();
}

extern "C" {

void init() {
  console_log(1001);
  n();
  drawUI();
  console_log(1002);
}

uint8_t *get_video_buffer() { return video_buffer; }

void set_mouse_pos(int x, int y) {
  input_mouse_x = x;
  input_mouse_y = y;
}

void set_mouse_button(int btn) { input_mouse_btn = btn; }

void update() {
  // 1. Mouse Update
  mouse.update();
  check();

  // 2. Logic Update
  if (game.rainmode && !game.paused) {
    for (int u = 1; u < 299; u++) {
      if (random_int(RAIN_PROBABILITY) == 1)
        field[u][1] = WATER_SPAWN_AMOUNT;
    }
  }

  if (mouse.rightDown == 1) {
    if (game.eraser == EraserMode::None)
      addWater();
    if (game.eraser == EraserMode::Wall)
      killWall();
    if (game.eraser == EraserMode::Water)
      killWater();
  }

  // Left click erasing (User Request)
  if (mouse.leftDown == 1) {
    if (game.eraser == EraserMode::Wall)
      killWall();
    if (game.eraser == EraserMode::Water)
      killWater();
  }

  // Drawing Logic (Only if not erasing)
  if (game.eraser == EraserMode::None) {
    if (game.drawmode == 1) { // Lines
      if ((mouse.leftDown == 1) && (mouse.oldLeftDown == 0) &&
          mouse.x < FIELD_WIDTH) {
        input.hasLeft = true;
        input.x1 = mouse.x;
        input.y1 = mouse.y;
      }
      if ((mouse.leftDown == 1) && (mouse.oldLeftDown == 1) && input.hasLeft &&
          mouse.x < FIELD_WIDTH) {
        // Preview logic would go here
      }
      if ((mouse.leftDown == 0) && (mouse.oldLeftDown == 1) && input.hasLeft &&
          mouse.x < FIELD_WIDTH) {
        input.hasLeft = false;
        logic_line(input.x1, input.y1, mouse.x, mouse.y, 1);
      }
    } else { // Free draw
      if (mouse.leftDown == 1 && mouse.oldLeftDown == 0 &&
          mouse.x < FIELD_WIDTH) {
        input.mayDraw = true;
        input.hasLeft = true;
        input.x1 = mouse.x;
        input.y1 = mouse.y;
      }
      if (mouse.leftDown == 1 && mouse.oldLeftDown == 1 && input.hasLeft &&
          input.mayDraw) {
        logic_line(input.x1, input.y1, mouse.x, mouse.y, 1);
        input.x1 = mouse.x;
        input.y1 = mouse.y;
      }
      if (mouse.leftDown == 0)
        input.mayDraw = false;
    }
  }

  game.frames++;

  // Simulation Step
  if (!game.paused) {
    for (int x = 1; x < 319; x++) {
      for (int y = 198; y > 0; y--) {
        if (field[x][y + 1] == 100)
          field[x][y] = 0; // Drain?

        if ((field[x][y] > 0) && (field[x][y] < 99)) {
          field[x][y]--; // Decay/Flow

          int u = field[x][y - 1];
          int d = field[x][y + 1];
          int l = field[x - 1][y];
          int r = field[x + 1][y];

          int q = d;
          int b = 2; // Default down

          // Water logic: find lowest neighbor
          if (u < q) {
            q = u;
            b = 1;
          } // Up? (Pressure?)
          if (l < q) {
            q = l;
            b = 3;
          }
          if (r < q) {
            q = r;
            b = 4;
          }

          // Move water
          if (b == 1 && field[x][y - 1] < 97)
            field[x][y - 1]++;
          if (b == 2 && field[x][y + 1] < 97)
            field[x][y + 1]++;
          if (b == 3 && field[x - 1][y] < 97)
            field[x - 1][y]++;
          if (b == 4 && field[x + 1][y] < 97)
            field[x + 1][y]++;
        }
      }
    }

    // Pass 2: Mass Conserving Flow (Backwards)
    // "DENSITY_FLOW" determines the rate of flow in this pass (originally k=2)
    for (int x = 318; x > 0; x--) {
      for (int y = 198; y >= 1; y--) {
        if ((field[x][y] > 0) &&
            (field[x][y] < 99)) { // Match original condition
          int u = field[x][y - 1];
          int d = field[x][y + 1];
          int l = field[x - 1][y];
          int r = field[x + 1][y];

          int q = d;
          int b = 2; // Default (down)

          if (l < q) {
            q = l;
            b = 3;
          }
          if (r < q) {
            q = r;
            b = 4;
          }
          if (u < q) {
            q = u;
            b = 1;
          }

          // Move water - use available amount to prevent sticking
          int flowAmt =
              (field[x][y] >= DENSITY_FLOW) ? DENSITY_FLOW : field[x][y];
          if (flowAmt > 0) {
            if (b == 1 && field[x][y - 1] < 97) {
              field[x][y - 1] += flowAmt;
              field[x][y] -= flowAmt;
            } else if (b == 2 && field[x][y + 1] < 97) {
              field[x][y + 1] += flowAmt;
              field[x][y] -= flowAmt;
            } else if (b == 3 && field[x - 1][y] < 97) {
              field[x - 1][y] += flowAmt;
              field[x][y] -= flowAmt;
            } else if (b == 4 && field[x + 1][y] < 97) {
              field[x + 1][y] += flowAmt;
              field[x][y] -= flowAmt;
            }
          }
        }
      }
    }
  }
}

void render() {
  // Redraw field to screen
  for (int x = 0; x < 300; x++) {
    for (int y = 0; y < 200; y++) {
      if (field[x][y] == 99) {
        putpixel(x, y, WHITE); // White wall
      } else if (field[x][y] != 0) {
        // Water color calculation
        putpixel(x, y, (field[x][y] + 1) / 2 + 103);
      } else {
        putpixel(x, y, 0);
      }
    }
  }

  // Redraw mouse
  if (mouse.x < 299) {
    int mx = mouse.x, my = mouse.y;
    putpixel(mx, my, 14);
    putpixel(mx + 1, my, 14);
    putpixel(mx, my + 1, 14);
    putpixel(mx, my - 1, 14);
    putpixel(mx - 1, my, 14);
  }
}

} // extern "C"
