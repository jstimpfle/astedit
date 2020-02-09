#include <astedit/astedit.h>
#include <astedit/zoom.h>

struct Zoom {
        int lineThicknessPx;
        int textHeightPx;
        int cellWidthPx;
        int fontSizePx;
};

static const struct Zoom zooms[] = {
        { 2, 15, -1, 8 },
        { 2, 16, -1, 9 },
        { 2, 17, -1, 10 },
        { 2, 18, -1, 11 },
        { 3, 19, -1, 12 },
        { 3, 21, -1, 13 },
        { 3, 22, -1, 14 },
        { 3, 24, -1, 16 },
        { 3, 25, -1, 18 },
};

int lineThicknessPx = 2;
int textHeightPx = 15;
int cellWidthPx = -1;
int fontSizePx = 10;

static int currentZoomIndex = 1;

static void set_zoom_index(int i)
{
        currentZoomIndex = i;
        lineThicknessPx = zooms[i].lineThicknessPx;
        textHeightPx = zooms[i].textHeightPx;
        cellWidthPx = zooms[i].cellWidthPx;
        fontSizePx = zooms[i].fontSizePx;
}

void increase_zoom(void)
{
        if (currentZoomIndex + 1 < LENGTH(zooms))
                set_zoom_index(currentZoomIndex + 1);
}

void decrease_zoom(void)
{
        if (currentZoomIndex > 0)
                set_zoom_index(currentZoomIndex - 1);
}
