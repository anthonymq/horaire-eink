#include "board_layout.h"
#include <algorithm>

static const int LEFT_ROW_TARGET_H = 64;     // Left boards: as many rows of this as fit (up to 5)
static const int COMPACT_ROW_H = 46;
static const int COMPACT_MAX_ROWS = 4;
static const int MAX_RIGHT_BOARDS = 3;

bool ScreenLayout::operator==(const ScreenLayout &o) const
{
    if (count != o.count || !(weather == o.weather)) return false;
    for (int i = 0; i < count; i++) {
        const BoardPlacement &a = boards[i], &b = o.boards[i];
        if (a.station != b.station || !(a.rect == b.rect) || a.rows != b.rows || a.rowH != b.rowH) return false;
    }
    return true;
}

ScreenLayout computeLayout(const bool *configured, const bool *large, int n)
{
    n = std::min(n, LAYOUT_MAX_BOARDS);
    int left[LAYOUT_MAX_BOARDS], right[LAYOUT_MAX_BOARDS];
    int nLeft = 0, nRight = 0;
    for (int i = 0; i < n; i++) {
        if (!configured[i]) continue;
        if (large[i]) left[nLeft++] = i;
        else right[nRight++] = i;
    }
    if (nLeft == 0 || nRight > MAX_RIGHT_BOARDS) {
        // Everything in the wide column, in slot order
        nLeft = nRight = 0;
        for (int i = 0; i < n; i++) {
            if (configured[i]) left[nLeft++] = i;
        }
    }

    ScreenLayout layout = {};
    const int bodyH = BODY_BOTTOM - BODY_Y;

    // Left column: boards share the height evenly
    if (nLeft > 0) {
        const int h = (bodyH - CARD_GAP * (nLeft - 1)) / nLeft;
        const int rows = std::max(2, std::min(MAX_BOARD_ROWS, (h - TITLE_H) / LEFT_ROW_TARGET_H));
        for (int i = 0; i < nLeft; i++) {
            BoardPlacement &b = layout.boards[layout.count++];
            b.station = left[i];
            b.rect = {LEFT_X, BODY_Y + i * (h + CARD_GAP), LEFT_W, h};
            b.rows = rows;
            b.rowH = (h - TITLE_H) / rows;
        }
    }

    // Right column: compact boards at their natural height, weather below
    int y = BODY_Y;
    if (nRight > 0) {
        const int avail = bodyH - WEATHER_MIN_H - CARD_GAP * nRight;
        const int per = std::min(avail / nRight, TITLE_H + COMPACT_MAX_ROWS * COMPACT_ROW_H);
        int rows = (per - TITLE_H) / COMPACT_ROW_H;
        int rowH = COMPACT_ROW_H;
        if (rows < 2) {
            rows = 2;
            rowH = (per - TITLE_H) / rows;
        }
        const int h = TITLE_H + rows * rowH;
        for (int i = 0; i < nRight; i++) {
            BoardPlacement &b = layout.boards[layout.count++];
            b.station = right[i];
            b.rect = {SIDE_X, y, SIDE_W, h};
            b.rows = rows;
            b.rowH = rowH;
            y += h + CARD_GAP;
        }
    }
    layout.weather = {SIDE_X, y, SIDE_W, BODY_BOTTOM - y};
    return layout;
}
