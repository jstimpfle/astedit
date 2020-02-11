#include <astedit/astedit.h>
#include <astedit/zoom.h>

struct Zoom {
        int textHeightPx;
        int lineHeightPx;
        int cellWidthPx;
        int borderWidthPx;
};

static const struct Zoom zooms[] = {
        { 8,  15,  6, 2, },
        { 9,  16,  7, 2, },
        { 10, 17,  8, 2, },
        { 12, 18,  9, 2, },
        { 14, 21, 10, 3, },
        { 16, 22, 11, 3, },
        { 18, 24, 12, 3, },
        { 20, 28, 13, 3, },
};

int textHeightPx = 12;
int lineHeightPx = 18;
int cellWidthPx = 9;
int borderWidthPx = 2;

static int currentZoomIndex = 3;

static void set_zoom_index(int i)
{
        currentZoomIndex = i;
        textHeightPx = zooms[i].textHeightPx;
        lineHeightPx = zooms[i].lineHeightPx;
        cellWidthPx = zooms[i].cellWidthPx;
        borderWidthPx = zooms[i].borderWidthPx;
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
