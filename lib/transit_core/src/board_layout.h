#pragma once

// Where each departure board goes on the 960x540 screen.
//
// Boards that show destination names ("large") stack in the wide left column;
// the others ("compact") stack in the right column above the weather, which
// takes whatever height is left. With no large board, every board goes left and
// the weather gets the whole right column (room for the forecast).
//
//  +--------------------------------------+------------------+
//  | header                                                  |
//  +--------------------------------------+------------------+
//  | large 1                              | compact 1        |
//  |                                      | compact 2        |
//  | large 2                              | weather          |
//  +--------------------------------------+------------------+

static const int LAYOUT_MAX_BOARDS = 4;

static const int SCREEN_WIDTH = 960;
static const int SCREEN_HEIGHT = 540;
static const int MARGIN = 20;
static const int HEADER_H = 44;
static const int BODY_Y = HEADER_H + 12;
static const int BODY_BOTTOM = SCREEN_HEIGHT - 12;
static const int TITLE_H = 32;    // Card title + rule
static const int CARD_GAP = 14;

static const int LEFT_X = MARGIN;
static const int LEFT_W = 540;   // Leaves the right column ~356 px: room for "Pluie vers 20h (70%)"
static const int DIVIDER_X = LEFT_X + LEFT_W + 12;
static const int SIDE_X = DIVIDER_X + 12;
static const int SIDE_W = SCREEN_WIDTH - MARGIN - SIDE_X;
static const int WEATHER_MIN_H = 104;

// Rows at least this tall get the big (60 px) countdown digits
static const int BIG_ROW_MIN_H = 76;
static const int MAX_BOARD_ROWS = 5;

struct LayoutRect {
    int x, y, w, h;
    bool operator==(const LayoutRect &o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};

struct BoardPlacement {
    int station;      // Index into the station slots
    LayoutRect rect;
    int rows;         // Departure rows below the title
    int rowH;
};

struct ScreenLayout {
    BoardPlacement boards[LAYOUT_MAX_BOARDS];
    int count;
    LayoutRect weather;

    bool operator==(const ScreenLayout &o) const;
    bool operator!=(const ScreenLayout &o) const { return !(*this == o); }
};

// `configured[i]` / `large[i]` for station slots 0..n-1 (n <= LAYOUT_MAX_BOARDS)
ScreenLayout computeLayout(const bool *configured, const bool *large, int n);
