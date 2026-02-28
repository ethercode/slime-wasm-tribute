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

/// Simulation field: each cell is water density (0-MAX_WATER), wall
/// (WALL_VALUE), or drain (DRAIN_VALUE)
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

/// Drawing mode
enum class DrawMode { Line = 1, Free = 2 };

// =============================================================================
// State Structs
// =============================================================================

/**
 * @brief Global game state variables
 */
struct GameState {
  EraserMode eraser = EraserMode::None; ///< Current eraser mode
  DrawMode drawmode = DrawMode::Line;   ///< Drawing tool mode
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

// =============================================================================
// VGA Palette
// =============================================================================

/// Standard 16-color VGA palette (RGB)
struct Color {
  uint8_t r, g, b;
};

constexpr Color VGA_PALETTE[16] = {
    {0, 0, 0},       // 0  Black
    {0, 0, 170},     // 1  Blue
    {0, 170, 0},     // 2  Green
    {0, 170, 170},   // 3  Cyan
    {170, 0, 0},     // 4  Red
    {170, 0, 170},   // 5  Magenta
    {170, 85, 0},    // 6  Brown
    {170, 170, 170}, // 7  Light Gray
    {85, 85, 85},    // 8  Dark Gray
    {85, 85, 255},   // 9  Light Blue
    {85, 255, 85},   // 10 Light Green
    {85, 255, 255},  // 11 Light Cyan
    {255, 85, 85},   // 12 Light Red
    {255, 85, 255},  // 13 Light Magenta
    {255, 255, 85},  // 14 Yellow
    {255, 255, 255}, // 15 White
};

// =============================================================================
// Graphics Implementation
// =============================================================================

/**
 * @brief Compute water color from density value
 *
 * Maps a density value (0-55 range) to a gradient:
 *   Blue (low/surface) → Magenta → Red → Orange → Yellow (high/deep)
 *
 * @param val Density value, clamped to [0, 55]
 * @return Color RGB for this density
 */
[[nodiscard]] constexpr Color waterColor(int val) noexcept {
  if (val < 0)
    val = 0;
  if (val > 55)
    val = 55;

  uint8_t r = 0, g = 0, b = 0;

  if (val < 14) {
    // Blue to Magenta (surface water)
    r = static_cast<uint8_t>((val * 255) / 14);
    g = 0;
    b = 255;
  } else if (val < 28) {
    // Magenta to Red (mid-depth)
    r = 255;
    g = 0;
    b = static_cast<uint8_t>(255 - ((val - 14) * 255) / 14);
  } else if (val < 42) {
    // Red to Orange (deeper)
    r = 255;
    g = static_cast<uint8_t>((val - 28) * 170 / 14);
    b = 0;
  } else {
    // Orange to Yellow (high pressure at bottom)
    r = 255;
    int gVal = 170 + ((val - 42) * 85) / 13;
    g = static_cast<uint8_t>(gVal > 255 ? 255 : gVal);
    b = 0;
  }

  return {r, g, b};
}

void putpixel(int x, int y, int c) {
  if (!inScreen(x, y))
    return;

  int index = (y * SCREEN_WIDTH + x) * 4;

  uint8_t r, g, b;

  if (c >= 100) {
    // Water simulation gradient
    Color wc = waterColor(c - 100);
    r = wc.r;
    g = wc.g;
    b = wc.b;
  } else {
    // Standard VGA palette
    const Color &pal = VGA_PALETTE[c % 16];
    r = pal.r;
    g = pal.g;
    b = pal.b;
  }

  video_buffer[index] = r;
  video_buffer[index + 1] = g;
  video_buffer[index + 2] = b;
  video_buffer[index + 3] = 255; // Alpha
}

// =============================================================================
// Line Drawing
// =============================================================================

/**
 * @brief Core line-drawing using a simple DDA algorithm
 *
 * Steps along the major axis and increments the minor axis proportionally.
 * Calls the provided callback for each pixel along the line.
 *
 * @tparam Callback Callable with signature void(int x, int y)
 * @param zx1 Start X
 * @param zy1 Start Y
 * @param zx2 End X
 * @param zy2 End Y
 * @param cb Callback invoked for each pixel
 */
template <typename Callback>
void walkLine(int zx1, int zy1, int zx2, int zy2, Callback cb) {
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

  if (x1 == x2 && y1 == y2)
    return;

  float dx = static_cast<float>(x2 - x1);
  float dy = static_cast<float>(y2 - y1);

  if (abs(static_cast<int>(dx)) > abs(static_cast<int>(dy))) {
    float iy = dy / dx;
    float cy = static_cast<float>(y1);
    if (x1 > x2)
      swap(x1, x2);
    for (int x = x1; x <= x2; x++) {
      cy += iy;
      cb(x, static_cast<int>(cy));
    }
  } else {
    float ix = abs(dx / dy);
    float cx = static_cast<float>(x1);
    if (y1 > y2) {
      swap(y1, y2);
      ix = -ix;
      cx = static_cast<float>(x2);
    }
    for (int y = y1; y <= y2; y++) {
      cx += ix;
      cb(static_cast<int>(cx), y);
    }
  }
}

void drawline(int x1, int y1, int x2, int y2, int col) {
  walkLine(x1, y1, x2, y2, [col](int x, int y) { putpixel(x, y, col); });
}

void line(int x1, int y1, int x2, int y2) { drawline(x1, y1, x2, y2, WHITE); }

/**
 * @brief Draw a line that modifies the simulation field (sets wall cells)
 *
 * For st == 1, marks field cells as walls.
 * For other st values, draws colored pixels (preview).
 */
void logic_line(int zx1, int zy1, int zx2, int zy2, int st) {
  walkLine(zx1, zy1, zx2, zy2, [st](int x, int y) {
    if (inField(x, y)) {
      if (st == 1)
        field[x][y] = WALL_VALUE;
      else
        putpixel(x, y, st);
    }
  });
}

void bar(int x1, int y1, int x2, int y2) {
  for (int x = x1; x <= x2; x++) {
    for (int y = y1; y <= y2; y++) {
      if (inScreen(x, y)) {
        putpixel(x, y, 7);
      }
    }
  }
}

// =============================================================================
// Game Logic Functions
// =============================================================================

/**
 * @brief Reset the simulation field to initial state
 *
 * Clears all cells and re-draws border walls.
 */
void resetField() {
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    for (int y = 0; y < SCREEN_HEIGHT; y++)
      field[x][y] = 0;
  }
  // Borders
  for (int x = 0; x < FIELD_WIDTH; x++) {
    field[x][0] = WALL_VALUE;
    field[x][FIELD_HEIGHT - 1] = WALL_VALUE;
  }
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    field[0][y] = WALL_VALUE;
    field[FIELD_WIDTH - 1][y] = WALL_VALUE;
  }
  game.rainmode = false;
}

void clearLines() {
  for (int x = 1; x < FIELD_WIDTH - 1; x++) {
    for (int y = 1; y < FIELD_HEIGHT - 1; y++) {
      if (field[x][y] == WALL_VALUE)
        field[x][y] = 0;
    }
  }
}

void clearWater() {
  for (int x = 0; x < FIELD_WIDTH; x++) {
    for (int y = 0; y < FIELD_HEIGHT; y++) {
      if (field[x][y] < WALL_VALUE)
        field[x][y] = 0;
    }
  }
  // Re-add borders
  for (int x = 0; x < FIELD_WIDTH; x++) {
    field[x][0] = WALL_VALUE;
    field[x][FIELD_HEIGHT - 1] = WALL_VALUE;
  }
  for (int y = 0; y < FIELD_HEIGHT; y++) {
    field[0][y] = WALL_VALUE;
    field[FIELD_WIDTH - 1][y] = WALL_VALUE;
  }
  game.rainmode = false;
}

void addWater() {
  int mmx = mouse.x;
  int mmy = mouse.y;
  for (int qx = -WATER_ADD_RADIUS; qx <= WATER_ADD_RADIUS; qx++) {
    for (int qy = -WATER_ADD_RADIUS * 2; qy < 0; qy++) {
      int px = qx + mmx;
      int py = qy + mmy;
      if (inFieldInterior(px, py)) {
        if (field[px][py] < WALL_VALUE) {
          field[px][py] = random_int(5);
        }
      }
    }
  }
}

void killWall() {
  int sx = mouse.x - 2;
  int sy = mouse.y - 2;
  for (int x = sx; x < sx + ERASER_SIZE; x++) {
    for (int y = sy; y < sy + ERASER_SIZE; y++) {
      if (inFieldInterior(x, y) && field[x][y] == WALL_VALUE) {
        field[x][y] = 0;
      }
    }
  }
}

void killWater() {
  int sx = mouse.x - 2;
  int sy = mouse.y - 2;
  for (int x = sx; x < sx + ERASER_SIZE; x++) {
    for (int y = sy; y < sy + ERASER_SIZE; y++) {
      if (inFieldInterior(x, y) && field[x][y] < WALL_VALUE) {
        field[x][y] = 0;
      }
    }
  }
}

// =============================================================================
// Tool / Button Handling
// =============================================================================

/**
 * @brief Select a single pencil/eraser tool, deselecting the others
 */
void selectTool(Tool tool) {
  buttons[static_cast<int>(Tool::Pencil)].isDown = (tool == Tool::Pencil);
  buttons[static_cast<int>(Tool::EraserWall)].isDown =
      (tool == Tool::EraserWall);
  buttons[static_cast<int>(Tool::EraserWater)].isDown =
      (tool == Tool::EraserWater);

  switch (tool) {
  case Tool::Pencil:
    game.eraser = EraserMode::None;
    break;
  case Tool::EraserWall:
    game.eraser = EraserMode::Wall;
    break;
  case Tool::EraserWater:
    game.eraser = EraserMode::Water;
    break;
  default:
    break;
  }
}

/**
 * @brief Select a draw mode, deselecting the other
 */
void selectDrawMode(DrawMode mode) {
  buttons[static_cast<int>(Tool::Line)].isDown = (mode == DrawMode::Line);
  buttons[static_cast<int>(Tool::Free)].isDown = (mode == DrawMode::Free);
  game.drawmode = mode;
}

void check() {
  for (int a = 0; a < 10; a++) {
    TButton &btn = buttons[a];

    // Handle tool switching on mouse-down edge
    if (mouse.leftDown && !mouse.oldLeftDown) {
      if (mouse.checkInside(btn.x1, btn.y1, btn.x2, btn.y2)) {
        switch (a) {
        case static_cast<int>(Tool::Pencil):
          selectTool(Tool::Pencil);
          break;
        case static_cast<int>(Tool::EraserWall):
          selectTool(Tool::EraserWall);
          break;
        case static_cast<int>(Tool::EraserWater):
          selectTool(Tool::EraserWater);
          break;
        case static_cast<int>(Tool::Line):
          selectDrawMode(DrawMode::Line);
          break;
        case static_cast<int>(Tool::Free):
          selectDrawMode(DrawMode::Free);
          break;
        default:
          break;
        }
      }
    }

    // Toggle actions on mouse-up edge
    if (mouse.checkInside(btn.x1, btn.y1, btn.x2, btn.y2)) {
      if (!mouse.leftDown && mouse.oldLeftDown && btn.isToggler) {
        switch (btn.tag) {
        case static_cast<int>(Action::Reset):
          resetField();
          break;
        case static_cast<int>(Action::Pause):
          game.paused = !game.paused;
          break;
        case static_cast<int>(Action::ClearLines):
          clearLines();
          break;
        case static_cast<int>(Action::ClearWater):
          clearWater();
          break;
        case static_cast<int>(Action::Rain):
          game.rainmode = !game.rainmode;
          break;
        default:
          break;
        }
        btn.isDown = false; // Flash effect
      }
    }
    btn.paint();
  }
}

// =============================================================================
// UI Setup
// =============================================================================

void drawUI() {
  static bool initialized = false;
  if (!initialized) {
    bar(SIDEBAR_X, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

    // --- Button Definitions ---
    auto setupBtn = [&](int i, int y1, int y2, int tag) {
      buttons[i].x1 = SIDEBAR_X + 1;
      buttons[i].y1 = y1;
      buttons[i].x2 = SCREEN_WIDTH - 2;
      buttons[i].y2 = y2;
      buttons[i].tag = tag;
    };

    setupBtn(0, 2, 19, static_cast<int>(Action::Rain));
    setupBtn(1, 27, 44, 0);   // Pencil
    setupBtn(2, 47, 64, 0);   // Eraser Wall
    setupBtn(3, 67, 84, 0);   // Eraser Water
    setupBtn(4, 92, 109, 0);  // Line
    setupBtn(5, 112, 129, 0); // Free
    setupBtn(6, 137, 154, static_cast<int>(Action::Pause));

    // Small Buttons
    setupBtn(8, 158, 166, static_cast<int>(Action::ClearLines));
    setupBtn(9, 169, 177, static_cast<int>(Action::ClearWater));

    setupBtn(7, 181, 198, static_cast<int>(Action::Reset));

    // Initial States
    buttons[static_cast<int>(Tool::Pencil)].isDown = true;
    buttons[static_cast<int>(Tool::Line)].isDown = true;

    for (auto &btn : buttons) {
      if (btn.tag >= 10)
        btn.isToggler = true;
    }

    // --- Icon Generation ---
    uint8_t icon[16][16];

    // Helper to fill icon
    auto fillIcon = [&](int color) {
      for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
          icon[x][y] = static_cast<uint8_t>(color);
    };

    // 0: Rain (Noise)
    fillIcon(0);
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++)
        if (random_int(5) == 1)
          icon[x][y] = 1;
    buttons[0].setBlitMap(icon);

    // 1: Pencil (Solid Blue)
    fillIcon(1);
    buttons[1].setBlitMap(icon);

    // 2: Eraser Wall (Gray box)
    fillIcon(0);
    for (int y = 3; y < 13; y++)
      for (int x = 3; x < 13; x++)
        icon[x][y] = WHITE;
    buttons[2].setBlitMap(icon);

    // 3: Eraser Water (Blue box)
    fillIcon(1);
    for (int y = 3; y < 13; y++)
      for (int x = 3; x < 13; x++)
        icon[x][y] = 11; // Light Cyan
    buttons[3].setBlitMap(icon);

    // 4: Line (Diagonal)
    fillIcon(0);
    for (int i = 0; i < 16; i++)
      icon[i][i] = WHITE;
    buttons[4].setBlitMap(icon);

    // 5: Free (Curve-ish parabola)
    fillIcon(0);
    for (int i = 0; i < 16; i++)
      icon[i][(i * i) / 16] = WHITE;
    buttons[5].setBlitMap(icon);

    // 6: Pause (Two bars)
    fillIcon(0);
    for (int y = 2; y < 14; y++) {
      icon[5][y] = 10;
      icon[6][y] = 10;
      icon[9][y] = 10;
      icon[10][y] = 10;
    }
    buttons[6].setBlitMap(icon);

    // 8: Clear Lines (Small Noise)
    fillIcon(TRANSPARENT_COLOR);
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 16; x++)
        icon[x][y] = static_cast<uint8_t>(random_int(2) ? WHITE : 0);
    buttons[8].setBlitMap(icon);

    // 9: Clear Water (Small Noise)
    fillIcon(TRANSPARENT_COLOR);
    for (int y = 0; y < 7; y++)
      for (int x = 0; x < 16; x++)
        icon[x][y] = static_cast<uint8_t>(random_int(2) ? 1 : 0);
    buttons[9].setBlitMap(icon);

    // 7: Reset (Cross)
    fillIcon(0);
    for (int i = 3; i < 13; i++) {
      icon[i][i] = 4;
      icon[i][15 - i] = 4;
    }
    buttons[7].setBlitMap(icon);

    initialized = true;
  }

  // Repaint all
  bar(SIDEBAR_X, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
  for (auto &btn : buttons)
    btn.paint();
}

// =============================================================================
// Exported C API
// =============================================================================

extern "C" {

void init() {
  console_log(1001);
  resetField();
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
    for (int u = 1; u < FIELD_WIDTH - 1; u++) {
      if (random_int(RAIN_PROBABILITY) == 1)
        field[u][1] = WATER_SPAWN_AMOUNT;
    }
  }

  if (mouse.rightDown) {
    switch (game.eraser) {
    case EraserMode::None:
      addWater();
      break;
    case EraserMode::Wall:
      killWall();
      break;
    case EraserMode::Water:
      killWater();
      break;
    }
  }

  // Left click erasing
  if (mouse.leftDown) {
    if (game.eraser == EraserMode::Wall)
      killWall();
    if (game.eraser == EraserMode::Water)
      killWater();
  }

  // Drawing Logic (Only if not erasing)
  if (game.eraser == EraserMode::None) {
    if (game.drawmode == DrawMode::Line) {
      if (mouse.leftDown && !mouse.oldLeftDown && mouse.x < FIELD_WIDTH) {
        input.hasLeft = true;
        input.x1 = mouse.x;
        input.y1 = mouse.y;
      }
      if (mouse.leftDown && mouse.oldLeftDown && input.hasLeft &&
          mouse.x < FIELD_WIDTH) {
        // Preview logic would go here
      }
      if (!mouse.leftDown && mouse.oldLeftDown && input.hasLeft) {
        input.hasLeft = false;
        logic_line(input.x1, input.y1, mouse.x, mouse.y, 1);
      }
    } else {
      // Free draw mode
      if (mouse.leftDown && !mouse.oldLeftDown && mouse.x < FIELD_WIDTH) {
        input.mayDraw = true;
        input.hasLeft = true;
        input.x1 = mouse.x;
        input.y1 = mouse.y;
      }
      if (mouse.leftDown && mouse.oldLeftDown && input.hasLeft &&
          input.mayDraw) {
        logic_line(input.x1, input.y1, mouse.x, mouse.y, 1);
        input.x1 = mouse.x;
        input.y1 = mouse.y;
      }
      if (!mouse.leftDown)
        input.mayDraw = false;
    }
  }

  game.frames++;

  // Simulation Step
  if (!game.paused) {
    // Pass 1: Forward flow with decay
    for (int x = 1; x < SCREEN_WIDTH - 1; x++) {
      for (int y = FIELD_HEIGHT - 2; y > 0; y--) {
        if (field[x][y + 1] == DRAIN_VALUE)
          field[x][y] = 0;

        if (field[x][y] > 0 && field[x][y] < WALL_VALUE) {
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
          }
          if (l < q) {
            q = l;
            b = 3;
          }
          if (r < q) {
            q = r;
            b = 4;
          }

          // Move water to lowest neighbor
          switch (b) {
          case 1:
            if (field[x][y - 1] < MAX_WATER)
              field[x][y - 1]++;
            break;
          case 2:
            if (field[x][y + 1] < MAX_WATER)
              field[x][y + 1]++;
            break;
          case 3:
            if (field[x - 1][y] < MAX_WATER)
              field[x - 1][y]++;
            break;
          case 4:
            if (field[x + 1][y] < MAX_WATER)
              field[x + 1][y]++;
            break;
          }
        }
      }
    }

    // Pass 2: Mass Conserving Flow (Backwards)
    for (int x = SCREEN_WIDTH - 2; x > 0; x--) {
      for (int y = FIELD_HEIGHT - 2; y >= 1; y--) {
        if (field[x][y] > 0 && field[x][y] < WALL_VALUE) {
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
            switch (b) {
            case 1:
              if (field[x][y - 1] < MAX_WATER) {
                field[x][y - 1] += flowAmt;
                field[x][y] -= flowAmt;
              }
              break;
            case 2:
              if (field[x][y + 1] < MAX_WATER) {
                field[x][y + 1] += flowAmt;
                field[x][y] -= flowAmt;
              }
              break;
            case 3:
              if (field[x - 1][y] < MAX_WATER) {
                field[x - 1][y] += flowAmt;
                field[x][y] -= flowAmt;
              }
              break;
            case 4:
              if (field[x + 1][y] < MAX_WATER) {
                field[x + 1][y] += flowAmt;
                field[x][y] -= flowAmt;
              }
              break;
            }
          }
        }
      }
    }
  }
}

void render() {
  // Redraw field to screen
  for (int x = 0; x < FIELD_WIDTH; x++) {
    for (int y = 0; y < FIELD_HEIGHT; y++) {
      if (field[x][y] == WALL_VALUE) {
        putpixel(x, y, WHITE);
      } else if (field[x][y] != 0) {
        // Water color calculation
        putpixel(x, y, (field[x][y] + 1) / 2 + 103);
      } else {
        putpixel(x, y, 0);
      }
    }
  }

  // Redraw mouse cursor
  if (mouse.x < FIELD_WIDTH - 1) {
    int mx = mouse.x, my = mouse.y;
    putpixel(mx, my, 14);
    putpixel(mx + 1, my, 14);
    putpixel(mx, my + 1, 14);
    putpixel(mx, my - 1, 14);
    putpixel(mx - 1, my, 14);
  }
}

} // extern "C"
