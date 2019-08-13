#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/window.h>
#include <astedit/font.h>
#include <astedit/gfx.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <astedit/utf8.h>
#include <astedit/draw2d.h>
#include <stdio.h> // snprintf
#include <string.h> // strlen()

static struct ColorVertex2d colorVertexBuffer[3 * 1024];
static struct TextureVertex2d alphaVertexBuffer[3 * 1024];

static int colorVertexCount;
static int alphaVertexCount;





static int minInt(int x, int y)
{
        return x < y ? x : y;
}



void flush_color_vertices(void)
{
        if (colorVertexCount > 0) {
                draw_rgba_vertices(colorVertexBuffer, colorVertexCount);
                colorVertexCount = 0;
        }
}

void flush_alpha_texture_vertices(void)
{
        if (alphaVertexCount > 0) {
                draw_alpha_texture_vertices(alphaVertexBuffer, alphaVertexCount);
                alphaVertexCount = 0;
        }
}

void push_color_vertices(struct ColorVertex2d *verts, int length)
{
        int i = 0;
        while (i < length) {
                int n = minInt(
                        length - i,
                        LENGTH(colorVertexBuffer) - colorVertexCount);
                COPY_ARRAY(colorVertexBuffer + colorVertexCount, verts + i, n);
                colorVertexCount += n;
                i += n;

                if (colorVertexCount == LENGTH(colorVertexBuffer))
                        flush_color_vertices();
        }
}

void push_alpha_texture_vertices(struct TextureVertex2d *verts, int length)
{
        flush_color_vertices();

        int i = 0;
        while (i < length) {
                int remainingBytes = LENGTH(alphaVertexBuffer) - alphaVertexCount;
                int n = length - i;
                if (n > remainingBytes)
                        n = remainingBytes;

                COPY_ARRAY(alphaVertexBuffer + alphaVertexCount, verts + i, n);
                alphaVertexCount += n;
                i += n;

                if (alphaVertexCount == LENGTH(alphaVertexBuffer))
                        flush_alpha_texture_vertices();
        }
}

void push_rgba_texture_vertices(struct TextureVertex2d *verts, int length)
{
        flush_color_vertices();
        flush_alpha_texture_vertices();
        draw_rgba_texture_vertices(verts, length);
}

void begin_frame(int x, int y, int w, int h)
{
        ENSURE(colorVertexCount == 0);
        ENSURE(alphaVertexCount == 0);
        set_viewport_in_pixels(x, y, w, h);
        set_2d_coordinate_system(x, y, w, h);
}

void end_frame(void)
{
        flush_color_vertices();
        flush_alpha_texture_vertices();
        //flush_rgba_texture_vertices();
}

static void fill_texture2d_rect(struct TextureVertex2d *rp,
        int r, int g, int b, int a,
        int x, int y, int w, int h,
        float texX, float texY, float texW, float texH)
{
        for (int i = 0; i < 6; i++) {
                rp[i].r = r / 255.0f;
                rp[i].g = g / 255.0f;
                rp[i].b = b / 255.0f;
                rp[i].a = a / 255.0f;
        }

        rp[0].x = (float) x;     rp[0].y = (float) y;       rp[0].z = 0;
        rp[1].x = (float) x;     rp[1].y = (float) y + h;   rp[1].z = 0;
        rp[2].x = (float) x + w; rp[2].y = (float) y + h;   rp[2].z = 0;
        rp[3].x = (float) x;     rp[3].y = (float) y;       rp[3].z = 0;
        rp[4].x = (float) x + w; rp[4].y = (float) y + h;   rp[4].z = 0;
        rp[5].x = (float) x + w; rp[5].y = (float) y;       rp[5].z = 0;

        rp[0].texX = texX;        rp[0].texY = texY;
        rp[1].texX = texX;        rp[1].texY = texY + texH;
        rp[2].texX = texX + texW; rp[2].texY = texY + texH;
        rp[3].texX = texX;        rp[3].texY = texY;
        rp[4].texX = texX + texW; rp[4].texY = texY + texH;
        rp[5].texX = texX + texW; rp[5].texY = texY;
}

void draw_colored_rect(int x, int y, int w, int h,
        unsigned r, unsigned g, unsigned b, unsigned a)
{
        static struct ColorVertex2d rectpoints[6];
        rectpoints[0].x = (float) x;     rectpoints[0].y = (float) y;       rectpoints[0].z = 0;
        rectpoints[1].x = (float) x;     rectpoints[1].y = (float) y + h;   rectpoints[1].z = 0;
        rectpoints[2].x = (float) x + w; rectpoints[2].y = (float) y + h;   rectpoints[2].z = 0;
        rectpoints[3].x = (float) x;     rectpoints[3].y = (float) y;       rectpoints[3].z = 0;
        rectpoints[4].x = (float) x + w; rectpoints[4].y = (float) y + h;   rectpoints[4].z = 0;
        rectpoints[5].x = (float) x + w; rectpoints[5].y = (float) y;       rectpoints[5].z = 0;
        for (int i = 0; i < 6; i++) {
                rectpoints[i].r = r / 255.0f;
                rectpoints[i].g = g / 255.0f;
                rectpoints[i].b = b / 255.0f;
                rectpoints[i].a = a / 255.0f;
        }
        push_color_vertices(rectpoints, LENGTH(rectpoints));
}

void draw_rgba_texture_rect(Texture texture,
        int x, int y, int w, int h,
        float texX, float texY, float texW, float texH)
{
        static struct TextureVertex2d rp[6];

        fill_texture2d_rect(&rp[0], 0, 0, 0, 0, /*TODO*/ x, y, w, h, texX, texY, texW, texH);

        for (int i = 0; i < 6; i++)
                rp[i].tex = texture;

        push_rgba_texture_vertices(rp, LENGTH(rp));
}

void draw_alpha_texture_rect(Texture texture,
        int r, int g, int b, int a,
        int x, int y, int w, int h,
        float texX, float texY, float texW, float texH)
{
        static struct TextureVertex2d rp[6];

        fill_texture2d_rect(&rp[0], r, g, b, a, x, y, w, h, texX, texY, texW, texH);

        for (int i = 0; i < 6; i++)
                rp[i].tex = texture;

        push_alpha_texture_vertices(rp, LENGTH(rp));
}

enum {
        DRAWSTRING_NORMAL,
        DRAWSTRING_HIGHLIGHT,
        DRAWSTRING_CURSOR_BEFORE,
};



void next_line(struct DrawCursor *cursor)
{
        cursor->x = cursor->xLeft;
        cursor->y += cursor->lineHeight;
}




static void draw_text_span(
        struct DrawCursor *cursor,
        const struct BoundingBox *boundingBox,
        const uint32_t *text,
        int start, int end, int drawstringKind,
        int r, int g, int b, int a)
{
        if (drawstringKind == DRAWSTRING_CURSOR_BEFORE)
                draw_colored_rect(cursor->x, cursor->y - cursor->ascender,
                        3, cursor->lineHeight,
                        192, 0, 0, 224);

        int i = start;
        while (i < end) {
                int j = i;
                while (j < end && text[j] != '\r' && text[j] != '\n')
                        j++;


                int xEnd = draw_glyphs_on_baseline(FONTFACE_REGULAR, boundingBox, 
                        cursor->fontSize, text + i, j - i,
                        cursor->x, cursor->y,
                        r, g, b, a);

                if (drawstringKind == DRAWSTRING_HIGHLIGHT) {
                        draw_colored_rect(cursor->x, cursor->y - cursor->ascender,
                                xEnd - cursor->x, cursor->lineHeight,
                                0, 0, 192, 32);
                }

                cursor->x = xEnd;

                i = j;

                if (i < end && text[i] == '\r')
                        i++;

                if (i < end && text[i] == '\n') {
                        i++;
                        next_line(cursor);
                }
        }
}

static void draw_codepoints_with_cursor(
        struct DrawCursor *cursor,
        const struct BoundingBox *boundingBox,
        const uint32_t *codepoints,
        int length, int markStart, int markEnd)
{
        int start = 0;
        while (start < length) {
                int numCodepointsRemain = length - start;
                int drawstringKind;
                int numCodepoints;
                int r, g, b, a;
                if (cursor->codepointpos < markStart) {
                        drawstringKind = DRAWSTRING_NORMAL;
                        numCodepoints = minInt(numCodepointsRemain, markStart - cursor->codepointpos);
                        r = 0; g = 0; b = 0; a = 255;
                }
                else if (cursor->codepointpos < markEnd) {
                        drawstringKind = DRAWSTRING_HIGHLIGHT;
                        numCodepoints = minInt(numCodepointsRemain, markEnd - cursor->codepointpos);
                        r = 0; g = 128; b = 0; a = 255;
                }
                else {
                        drawstringKind = DRAWSTRING_NORMAL;
                        numCodepoints = numCodepointsRemain;
                        r = 0; g = 0; b = 0; a = 255;
                }
                draw_text_span(cursor, boundingBox, codepoints, start, start + numCodepoints, drawstringKind,
                        r, g, b, a);
                start += numCodepoints;
                cursor->codepointpos += numCodepoints;
        }
        /* quick hack to get cursor at end */
        if (start == markStart)
                draw_text_span(cursor, boundingBox, codepoints, start, 0, DRAWSTRING_CURSOR_BEFORE,
                        0, 128, 0, 255);
}

static void draw_text_with_cursor(
        struct DrawCursor *cursor,
        const struct BoundingBox *boundingBox,
        const char *text, int length,
        int markStart, int markEnd)
{
        uint32_t codepoints[512];

        int i = 0;
        while (i < length) {
                int n = LENGTH(codepoints);
                if (n > length - i)
                        n = length - i;

                int numCodepointsDecoded;
                decode_utf8_span(text, i, length, codepoints, LENGTH(codepoints), &i, &numCodepointsDecoded);

                draw_codepoints_with_cursor(cursor, boundingBox,
                        codepoints, numCodepointsDecoded,
                        markStart, markEnd);
        }
}

static void draw_text_snprintf(
        struct DrawCursor *cursor,
        const struct BoundingBox *boundingBox,
        char *buffer, int length,
        const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        int numBytes = vsnprintf(buffer, length, fmt, ap);
        if (numBytes >= 0)
                draw_text_with_cursor(cursor, boundingBox, buffer, numBytes, -1, -1);
        va_end(ap);
}


static const int LINE_HEIGHT = 33;


static void draw_line_numbers(struct TextEdit *edit, int firstLine, int numberOfLines, int x, int y, int w, int h)
{
        UNUSED(edit);

        struct BoundingBox box;
        box.bbX = x;
        box.bbY = y;
        box.bbW = w;
        box.bbH = h;

        struct DrawCursor drawCursor;
        drawCursor.xLeft = x;
        drawCursor.fontSize = 25;
        drawCursor.ascender = 20;
        drawCursor.lineHeight = LINE_HEIGHT;
        drawCursor.x = x;
        drawCursor.y = y + drawCursor.lineHeight;
        drawCursor.codepointpos = 0;
        drawCursor.lineNumber = 0;
        struct DrawCursor *cursor = &drawCursor;

        int onePastLastLine = firstLine + numberOfLines;

        for (int i = firstLine; i < onePastLastLine; i++) {
                char buf[16];
                snprintf(buf, sizeof buf, "%4d", i + 1);
                draw_text_with_cursor(cursor, &box, buf, (int) strlen(buf), -1, -1);
                next_line(cursor);
        }
}

static void draw_textedit_lines(struct TextEdit *edit, int firstLine, int numberOfLines,
        int x, int y, int w, int h, int markStart, int markEnd)
{
        char readbuffer[256];
        uint32_t codepointBuffer[LENGTH(readbuffer)];

        int readbufferFill = 0;

        int readPositionInBytes = compute_pos_of_line(edit->rope, firstLine);
        int lastPositionInBytes = compute_pos_of_line(edit->rope, firstLine + numberOfLines); //XXX

        struct BoundingBox box;
        box.bbX = x;
        box.bbY = y;
        box.bbW = w;
        box.bbH = h;
        struct BoundingBox *boundingBox = &box;

        struct DrawCursor cursor;
        cursor.xLeft = x;
        cursor.fontSize = 25;
        cursor.ascender = 20;
        cursor.lineHeight = LINE_HEIGHT;
        cursor.x = x;
        cursor.y = y + cursor.lineHeight;
        cursor.codepointpos = compute_codepoint_position(edit->rope, readPositionInBytes);
        cursor.lineNumber = firstLine;

        while (cursor.y - cursor.ascender < boundingBox->bbY + boundingBox->bbH) {
                if (readPositionInBytes >= lastPositionInBytes)
                        break;

                {
                        int remainingSpace = LENGTH(readbuffer) - readbufferFill;
                        int editBytesAvailable = lastPositionInBytes - readPositionInBytes;

                        int maxReadLength = remainingSpace;
                        if (maxReadLength > editBytesAvailable)
                                maxReadLength = editBytesAvailable;

                        int numBytesRead = copy_text_from_textrope(edit->rope, readPositionInBytes,
                                readbuffer + readbufferFill, maxReadLength);
                        readbufferFill += numBytesRead;
                        readPositionInBytes += numBytesRead;

                        if (numBytesRead == 0)
                                /* EOF. Ignore remaining undecoded bytes */
                                break;
                }

                int numCodepointsDecoded;

                decode_utf8_span_and_move_rest_to_front(readbuffer, readbufferFill,
                        codepointBuffer, &readbufferFill, &numCodepointsDecoded);

                draw_codepoints_with_cursor(&cursor, boundingBox,
                        codepointBuffer, numCodepointsDecoded,
                        markStart, markEnd);
        }
}

static void draw_textedit_statusline(struct TextEdit *edit, int x, int y, int w, int h)
{

        // no actual bounding box currently.
        struct BoundingBox box;
        box.bbX = 0;
        box.bbY = 0;
        box.bbW = windowWidthInPixels;
        box.bbH = windowHeightInPixels;

        struct DrawCursor cursor;
        cursor.xLeft = x;
        cursor.fontSize = 25;
        cursor.ascender = 20;
        cursor.lineHeight = LINE_HEIGHT;
        cursor.x = x;
        cursor.y = y + LINE_HEIGHT*2/3;
        cursor.codepointpos = 0;
        cursor.lineNumber = 0;

        int pos = edit->cursorBytePosition;
        int codepointPos = compute_codepoint_position(edit->rope, pos);
        int lineNumber = compute_line_number(edit->rope, pos);
        char textbuffer[512];

        draw_colored_rect(x, y, w, h, 128, 160, 128, 224);

        draw_text_snprintf(&cursor, &box, textbuffer, sizeof textbuffer, "pos: %d", pos);
        draw_text_snprintf(&cursor, &box, textbuffer, sizeof textbuffer, ", codepointPos: %d", codepointPos);
        draw_text_snprintf(&cursor, &box, textbuffer, sizeof textbuffer, ", lineNumber: %d", lineNumber);
        draw_text_snprintf(&cursor, &box, textbuffer, sizeof textbuffer, ", selecting?: %d", edit->isSelectionMode);
}

static void draw_textedit_loading(struct TextEdit *edit, int x, int y, int w, int h)
{

        // no actual bounding box currently.
        struct BoundingBox box;
        box.bbX = 0;
        box.bbY = 0;
        box.bbW = windowWidthInPixels;
        box.bbH = windowHeightInPixels;

        struct DrawCursor cursor;
        cursor.xLeft = x;
        cursor.fontSize = 25;
        cursor.ascender = 20;
        cursor.lineHeight = LINE_HEIGHT;
        cursor.x = x;
        cursor.y = y + LINE_HEIGHT * 2 / 3;
        cursor.codepointpos = 0;
        cursor.lineNumber = 0;

        char textbuffer[512];

        draw_colored_rect(x, y, w, h, 128, 160, 128, 224);

        draw_text_snprintf(&cursor, &box, textbuffer, sizeof textbuffer,
                "Loading %d%%  (%d / %d bytes)",
                (int) ((long long) edit->loadingCompletedBytes * 100
                        / (edit->loadingTotalBytes ? edit->loadingTotalBytes : 1)),
                edit->loadingCompletedBytes,
                edit->loadingTotalBytes);
}


static void draw_TextEdit(int canvasX, int canvasY, int canvasW, int canvasH,
        struct TextEdit *edit, int firstLine, int markStart, int markEnd)
{
        int statusLineH = 40;
        
        int linesX = canvasX;
        int linesY = canvasY;
        int linesW = 200;
        int linesH = canvasH < statusLineH ? 0 : canvasH - statusLineH;

        int textAreaX = linesX + linesW;
        int textAreaY = linesY;
        int textAreaW = canvasW < linesW ? 0 : canvasW - linesW;
        int textAreaH = linesH;

        int statusLineX = linesX;
        int statusLineY = linesY + linesH;
        int statusLineW = canvasW;

        draw_colored_rect(0, 0, windowWidthInPixels, windowHeightInPixels, 224, 224, 224, 255);

        if (edit->isLoading) {
                draw_textedit_loading(edit, statusLineX, statusLineY, statusLineW, statusLineH);
        } else {
                int length = textrope_length(edit->rope);

                if (markStart < 0) markStart = 0;
                if (markEnd < 0) markEnd = 0;
                if (markStart >= length) markStart = length;
                if (markEnd >= length) markEnd = length;

                ENSURE(markStart <= markEnd);

                int maxNumberOfLines = (textAreaH + LINE_HEIGHT - 1) / LINE_HEIGHT;
                int numberOfLines = textrope_number_of_lines_quirky(edit->rope) - firstLine;
                if (numberOfLines > maxNumberOfLines)
                        numberOfLines = maxNumberOfLines;

                // XXX is here the right place to do this?
                edit->numberOfLinesDisplayed = textAreaH / LINE_HEIGHT;

                draw_line_numbers(edit, firstLine, numberOfLines, linesX, linesY, linesW, linesH);
                draw_textedit_lines(edit, firstLine, numberOfLines, textAreaX, textAreaY, textAreaW, textAreaH, markStart, markEnd);
                draw_textedit_statusline(edit, statusLineX, statusLineY, statusLineW, statusLineH);
        }
}

static int maxInt(int x, int y) { return x > y ? x : y; }

void testdraw(struct TextEdit *edit)
{
        int markStart;
        int markEnd;
        if (edit->isSelectionMode)
                get_selected_range_in_codepoints(edit, &markStart, &markEnd);
        else {
                markStart = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
                markEnd = markStart + 1;
        }

        int canvasX = 100;
        int canvasY = 100;
        int canvasW = maxInt(windowWidthInPixels - 200, 0);
        int canvasH = maxInt(windowHeightInPixels - 200, 0);

        clear_screen_and_drawing_state();
        begin_frame(canvasX, canvasY, canvasW, canvasH);

        draw_TextEdit(0, 0, canvasW, canvasH,
                edit, edit->firstLineDisplayed,
                markStart, markEnd);

        end_frame();
}
