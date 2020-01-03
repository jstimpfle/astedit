#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/editor.h>
#include <astedit/logging.h>
#include <astedit/window.h>
#include <astedit/font.h>
#include <astedit/gfx.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <astedit/utf8.h>
#include <astedit/textropeUTF8decode.h>
#include <astedit/draw2d.h>
#include <blunt/lex.h>
#include <stdio.h> // snprintf
#include <string.h> // strlen()

struct RGB { unsigned r, g, b; };
#define C(x) x.r, x.g, x.b, 255
#define C_ALPHA(x, a) x.r, x.g, x.b, a

#if 1
static const struct RGB texteditBgColor = { 0, 0, 0 };
static const struct RGB statusbarBgColor = { 128, 160, 128 };
static const struct RGB normalTextColor = {0, 255, 0 };
static const struct RGB stringTokenColor = { 255, 255, 0 };
static const struct RGB integerTokenColor = { 255, 255, 0 };
static const struct RGB operatorTokenColor = { 255, 255, 0 };  // same as normalTextColor, but MSVC won't let me do that ("initializer is not a constant")
static const struct RGB junkTokenColor = { 255, 0, 0 };
static const struct RGB commentTokenColor = { 0, 0, 255 };
static const struct RGB borderColor = { 32, 32, 32 };
static const struct RGB highlightColor = { 0, 0, 255 };
static const struct RGB cursorColor = { 0, 255, 255 };
static const struct RGB currentLineBorderColor = { 32, 32, 32 };
static const struct RGB statusbarTextColor = { 0, 0, 0 };
#else
static const struct RGB texteditBgColor = { 255, 255, 255 };
static const struct RGB statusbarBgColor = { 64, 64, 64 };
static const struct RGB normalTextColor = {0, 0, 0 };
static const struct RGB stringTokenColor = { 32, 160, 32 };
static const struct RGB integerTokenColor = { 0, 0, 255 };
static const struct RGB operatorTokenColor = {0, 0, 255};  // same as normalTextColor, but MSVC won't let me use that ("initializer is not a constant")
static const struct RGB junkTokenColor = { 255, 0, 0 };
static const struct RGB commentTokenColor = { 0, 255, 255 };
static const struct RGB borderColor = { 128, 128, 128 };
static const struct RGB highlightColor = { 192, 192, 255 };
static const struct RGB cursorColor = { 16, 16, 32 };
static const struct RGB currentLineBorderColor = { 32, 32, 32 };
static const struct RGB statusbarTextColor = { 224, 224, 224 };
#endif

// TODO: These values should be dynamically computed. We want to support various
// a wide range of monitor resolutions.
static const int LINE_THICKNESS_PIXELS = 2;

static const int TEXT_HEIGHT_PIXELS = 26;
static const int CELL_WIDTH_PIXELS = -1;//22;
static const int FONT_SIZE_PIXELS = 14;

static struct ColorVertex2d colorVertexBuffer[3 * 1024];
static struct TextureVertex2d subpixelRenderedFontVertexBuffer[3 * 1024];

static int colorVertexCount;
static int subpixelRenderedFontVertexCount;


/* The drawing routines here will never paint solid color above font.
 * We exploit that here by flushing them in the following order,
 * to allow the painting to paint in interleaved order. */
void flush_all_vertex_buffers(void)
{
        if (colorVertexCount > 0) {
                draw_rgba_vertices(colorVertexBuffer, colorVertexCount);
                colorVertexCount = 0;
        }
        if (subpixelRenderedFontVertexCount > 0) {
                draw_subpixelRenderedFont_vertices(subpixelRenderedFontVertexBuffer, subpixelRenderedFontVertexCount);
                subpixelRenderedFontVertexCount = 0;
        }
}

void push_color_vertices(struct ColorVertex2d *verts, int length)
{
        int i = 0;
        while (i < length) {
                int n = LENGTH(colorVertexBuffer) - colorVertexCount;
                if (n > length - i)
                        n = length - i;
                COPY_ARRAY(colorVertexBuffer + colorVertexCount, verts + i, n);
                colorVertexCount += n;
                i += n;

                if (colorVertexCount == LENGTH(colorVertexBuffer))
                        flush_all_vertex_buffers();
        }
}

void push_subpixelRenderedFont_texture_vertices(struct TextureVertex2d *verts, int length)
{
        int i = 0;
        while (i < length) {
                int remainingBytes = LENGTH(subpixelRenderedFontVertexBuffer) - subpixelRenderedFontVertexCount;
                int n = length - i;
                if (n > remainingBytes)
                        n = remainingBytes;

                COPY_ARRAY(subpixelRenderedFontVertexBuffer + subpixelRenderedFontVertexCount, verts + i, n);
                subpixelRenderedFontVertexCount += n;
                i += n;

                if (subpixelRenderedFontVertexCount == LENGTH(subpixelRenderedFontVertexBuffer))
                        flush_all_vertex_buffers();
        }
}

/*
void push_rgba_texture_vertices(struct TextureVertex2d *verts, int length)
{
        flush_color_vertex_buffers();
        draw_rgba_texture_vertices(verts, length);
}
*/

void begin_frame(int x, int y, int w, int h)
{
        ENSURE(colorVertexCount == 0);
        ENSURE(subpixelRenderedFontVertexCount == 0);
        set_viewport_in_pixels(x, y, w, h);
        set_2d_coordinate_system(x, y, w, h);
}

void end_frame(void)
{
        flush_all_vertex_buffers();
}

static void fill_texture2d_rect(struct TextureVertex2d *rp,
        int r, int g, int b, int a,
        int x, int y, int w, int h,
        int texX, int texY, int texW, int texH)
{
        for (int i = 0; i < 6; i++) {
                rp[i].r = r / 255.0f;
                rp[i].g = g / 255.0f;
                rp[i].b = b / 255.0f;
                rp[i].a = a / 255.0f;
        }

        rp[0].x = (float) x;     rp[0].y = (float) y;       rp[0].z = 0.0f;
        rp[1].x = (float) x;     rp[1].y = (float) y + h;   rp[1].z = 0.0f;
        rp[2].x = (float) x + w; rp[2].y = (float) y + h;   rp[2].z = 0.0f;
        rp[3].x = (float) x;     rp[3].y = (float) y;       rp[3].z = 0.0f;
        rp[4].x = (float) x + w; rp[4].y = (float) y + h;   rp[4].z = 0.0f;
        rp[5].x = (float) x + w; rp[5].y = (float) y;       rp[5].z = 0.0f;

        //for (int i = 0; i < 6; i++) rp[i].x += 0.5f;

        rp[0].texX = (float) texX;        rp[0].texY = (float) texY;
        rp[1].texX = (float) texX;        rp[1].texY = (float) texY + texH;
        rp[2].texX = (float) texX + texW; rp[2].texY = (float) texY + texH;
        rp[3].texX = (float) texX;        rp[3].texY = (float) texY;
        rp[4].texX = (float) texX + texW; rp[4].texY = (float) texY + texH;
        rp[5].texX = (float) texX + texW; rp[5].texY = (float) texY;
}

void draw_colored_rect(int x, int y, int w, int h,
        unsigned r, unsigned g, unsigned b, unsigned a)
{
        static struct ColorVertex2d rectpoints[6];
        rectpoints[0].x = (float) x;     rectpoints[0].y = (float) y;       rectpoints[0].z = 0.0f;
        rectpoints[1].x = (float) x;     rectpoints[1].y = (float) y + h;   rectpoints[1].z = 0.0f;
        rectpoints[2].x = (float) x + w; rectpoints[2].y = (float) y + h;   rectpoints[2].z = 0.0f;
        rectpoints[3].x = (float) x;     rectpoints[3].y = (float) y;       rectpoints[3].z = 0.0f;
        rectpoints[4].x = (float) x + w; rectpoints[4].y = (float) y + h;   rectpoints[4].z = 0.0f;
        rectpoints[5].x = (float) x + w; rectpoints[5].y = (float) y;       rectpoints[5].z = 0.0f;
        for (int i = 0; i < 6; i++) {
                rectpoints[i].r = r / 255.0f;
                rectpoints[i].g = g / 255.0f;
                rectpoints[i].b = b / 255.0f;
                rectpoints[i].a = a / 255.0f;
        }
        push_color_vertices(rectpoints, LENGTH(rectpoints));
}

#if 0
void draw_rgba_texture_rect(Texture texture,
        int x, int y, int w, int h,
        int texX, int texY, int texW, int texH)
{
        static struct TextureVertex2d rp[6];

        fill_texture2d_rect(&rp[0], 0, 0, 0, 0, /*TODO*/ x, y, w, h, texX, texY, texW, texH);

        for (int i = 0; i < 6; i++)
                rp[i].tex = texture;

        push_rgba_texture_vertices(rp, LENGTH(rp));
}
#endif

void draw_subpixelRenderedFont_texture_rect(Texture texture,
        int r, int g, int b, int a,
        int x, int y, int w, int h,
        int texX, int texY, int texW, int texH)
{
        static struct TextureVertex2d rp[6];

        fill_texture2d_rect(&rp[0], r, g, b, a, x, y, w, h, texX, texY, texW, texH);

        for (int i = 0; i < 6; i++)
                rp[i].tex = texture;

        push_subpixelRenderedFont_texture_vertices(rp, LENGTH(rp));
}

static void draw_horizontal_line(int x, int y, int w)
{
        if (w < 0) w = 0;
        y -= LINE_THICKNESS_PIXELS / 2;
        int h = LINE_THICKNESS_PIXELS - (LINE_THICKNESS_PIXELS / 2);
        draw_colored_rect(x, y, w, h, C(borderColor));
}

static void draw_vertical_line(int x, int y, int h)
{
        if (h < 0) h = 0;
        x -= LINE_THICKNESS_PIXELS / 2;
        int w = LINE_THICKNESS_PIXELS - (LINE_THICKNESS_PIXELS / 2);
        draw_colored_rect(x, y, w, h, C(borderColor));
}

void draw_colored_border(int x, int y, int w, int h, int thicknessPerSide,
        int r, int g, int b, int a)
{
        int t = thicknessPerSide;
        draw_colored_rect(x - t, y - t, w + 2 * t, 1 + 2 * t,          r, g, b, a);
        draw_colored_rect(x - t, y + h - t, w + 2 * t, 1 + 2 * t,      r, g, b, a);
        draw_colored_rect(x - t, y - t, 1 + 2 * t, h + 2 * t,          r, g, b, a);
        draw_colored_rect(x + w - t, y - t, 1 + 2 * t, h + 2 * t,      r, g, b, a);
}

enum {
        DRAWSTRING_NORMAL,
        DRAWSTRING_HIGHLIGHT,
        DRAWSTRING_HIGHLIGHT_BORDERS,
};



void next_line(struct DrawCursor *cursor)
{
        cursor->x = cursor->xLeft;
        cursor->lineY += cursor->lineHeight;
}

static void draw_codepoint(
        struct DrawCursor *cursor,
        const struct GuiRect *boundingBox,
        int drawstringKind,
        uint32_t codepoint)
{
        int xEnd = draw_glyphs_on_baseline(FONTFACE_REGULAR, boundingBox,
                cursor->fontSize, cursor->cellWidth, &codepoint, 1,
                cursor->x, cursor->lineY + cursor->distanceYtoBaseline,
                cursor->r, cursor->g, cursor->b, cursor->a);
        if (drawstringKind == DRAWSTRING_HIGHLIGHT)
                draw_colored_rect(cursor->x, cursor->lineY,
                        xEnd - cursor->x, cursor->lineHeight, C(highlightColor));
        else if (drawstringKind == DRAWSTRING_HIGHLIGHT_BORDERS)
                draw_colored_border(cursor->x, cursor->lineY,
                        xEnd - cursor->x, cursor->lineHeight, 2, C(highlightColor));
        cursor->x = xEnd;
}

static void draw_text_with_cursor(
        struct DrawCursor *cursor,
        const struct GuiRect *boundingBox,
        const char *text, int length)
{
        uint32_t codepoints[512];

        int i = 0;
        while (i < length) {
                int n = LENGTH(codepoints);
                if (n > length - i)
                        n = length - i;

                int numCodepointsDecoded;
                decode_utf8_span(text, i, length, codepoints, LENGTH(codepoints), &i, &numCodepointsDecoded);

                for (int j = 0; j < numCodepointsDecoded; j++)
                        draw_codepoint(cursor, boundingBox, DRAWSTRING_NORMAL, codepoints[j]);
        }
}

static void draw_text_snprintf(
        struct DrawCursor *cursor,
        const struct GuiRect *boundingBox,
        char *buffer, int length,
        const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        int numBytes = vsnprintf(buffer, length, fmt, ap);
        if (numBytes >= 0)
                draw_text_with_cursor(cursor, boundingBox, buffer, numBytes);
        va_end(ap);
}

static void set_bounding_box(struct GuiRect *box, int x, int y, int w, int h)
{
        box->x = x;
        box->y = y;
        box->w = w;
        box->h = h;
}


static void set_draw_cursor(struct DrawCursor *cursor, int x, int y, FILEPOS codepointPos, FILEPOS lineNumber)
{
        cursor->xLeft = x;
        cursor->fontSize = FONT_SIZE_PIXELS;
        cursor->distanceYtoBaseline = TEXT_HEIGHT_PIXELS * 2 / 3;
        cursor->lineHeight = TEXT_HEIGHT_PIXELS;
        cursor->cellWidth = CELL_WIDTH_PIXELS;
        cursor->x = x;
        cursor->lineY = y;
        cursor->lineNumber = lineNumber;
}

static void set_cursor_color(struct DrawCursor *cursor, int r, int g, int b, int a)
{
        cursor->r = r;
        cursor->g = g;
        cursor->b = b;
        cursor->a = a;
}


static void draw_line_numbers(struct TextEdit *edit, FILEPOS firstVisibleLine, int offsetPixelsY, int x, int y, int w, int h)
{
        FILEPOS lastLine = textrope_number_of_lines_quirky(edit->rope);

        struct GuiRect boundingBox;
        struct DrawCursor drawCursor;
        struct GuiRect *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, x, y, w, h);
        set_draw_cursor(cursor, x, y - offsetPixelsY, 0, 0);

        set_cursor_color(cursor, C(normalTextColor));

        for (FILEPOS i = firstVisibleLine;
             i < lastLine && cursor->lineY < box->y + box->h;
             i++) {
                char buf[32];
                snprintf(buf, sizeof buf, "%4"FILEPOS_PRI, i + 1);
                draw_text_with_cursor(cursor, &boundingBox, buf, (int) strlen(buf));
                next_line(cursor);
        }
}

static void draw_cursor(struct TextEdit *edit, int x, int y)
{
        //XXX
        int w = 1;
        int h = TEXT_HEIGHT_PIXELS;
        if (edit->vistate.vimodeKind == VIMODE_INPUT)
                draw_colored_border(x, y, w, h, 2, C(cursorColor));
        else
                draw_colored_border(x, y, w, h, 1, C(cursorColor));
}

static void draw_textedit_lines(struct TextEdit *edit,
        FILEPOS firstVisibleLine, int offsetPixelsY,
        int x, int y, int w, int h)
{
        { // we want to add a tiny little bit of pad
                int pad = 5;
                x += pad;
                y += pad;
                w -= 2 * pad;
                h -= 2 * pad;
        }

        struct GuiRect boundingBox;
        struct GuiRect *box = &boundingBox;
        set_bounding_box(box, x, y, w, h);

        struct TextropeUTF8Decoder decoder;
        struct Blunt_ReadCtx readCtx;
        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;
        {
                /* We can start lexing at the first character of the line
                only because the lexical syntax is made such that newline is
                always a token separator. Otherwise, we'd need to find the start
                a little before that line and with more complicated code. */
                FILEPOS initialReadPos = compute_pos_of_line(edit->rope, firstVisibleLine);
                FILEPOS initialCodepointPos = compute_codepoint_position(edit->rope, initialReadPos);
                init_UTF8Decoder(&decoder, edit->rope, initialReadPos);
                begin_lexing_blunt_tokens(&readCtx, edit->rope, initialReadPos);
                set_draw_cursor(cursor, x, y - offsetPixelsY, initialCodepointPos, firstVisibleLine);
        }

        // AAARGH, we shouldn't call the drawing context "cursor"
        FILEPOS markStart = 0;
        FILEPOS markEnd = 0;
        {
                FILEPOS textropeLength = textrope_length(edit->rope);
                if (edit->isSelectionMode)
                        get_selected_range_in_codepoints(edit, &markStart, &markEnd);
                if (markStart < 0) markStart = 0;
                if (markEnd < 0) markEnd = 0;
                if (markStart >= textropeLength) markStart = textropeLength;
                if (markEnd >= textropeLength) markEnd = textropeLength;
        }
        ENSURE(markStart <= markEnd);

        { // draw box of line width where the cursor is.
        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;
        // draw a rectangular border around the text line that has cursor
        FILEPOS currentLine = compute_line_number(edit->rope, edit->cursorBytePosition);
        int linesDiff = cast_filepos_to_int(currentLine - firstVisibleLine);
        set_draw_cursor(cursor, x, y, 0, 0); //XXX extract the computation for line geometry
        for (int i = 0; i < linesDiff; i++)
                next_line(cursor);
        int borderX = x;
        int borderY = cursor->lineY;
        int borderH = cursor->lineHeight;
        int borderW = w;
        int borderThickness = 1;
        draw_colored_border(borderX, borderY, borderW, borderH, borderThickness, C_ALPHA(currentLineBorderColor, 128));
        }

        while (cursor->lineY < box->y + box->h) {
                struct Blunt_Token token;
                lex_blunt_token(&readCtx, &token);

                FILEPOS currentPos = readpos_in_bytes_of_UTF8Decoder(&decoder);
                //FILEPOS whiteEndPos = currentPos + token.leadingWhiteChars;
                FILEPOS tokenEndPos = currentPos + token.length;
                struct RGB rgb;
                if (token.tokenKind == BLUNT_TOKEN_INTEGER)
                        rgb = integerTokenColor;
                else if (token.tokenKind == BLUNT_TOKEN_STRING)
                        rgb = stringTokenColor;
                else if (FIRST_BLUNT_TOKEN_OPERATOR <= token.tokenKind
                        && token.tokenKind <= LAST_BLUNT_TOKEN_OPERATOR)
                        rgb = operatorTokenColor;
                else if (token.tokenKind == BLUNT_TOKEN_COMMENT)
                        rgb = commentTokenColor;
                else if (token.tokenKind == BLUNT_TOKEN_JUNK)
                        rgb = junkTokenColor;
                else
                        rgb = normalTextColor;
                set_cursor_color(cursor, C(rgb));
                for (;;) {
                        FILEPOS readpos = readpos_in_bytes_of_UTF8Decoder(&decoder);
                        if (readpos >= tokenEndPos)
                                break;
                        if (readpos == edit->cursorBytePosition)
                                draw_cursor(edit, cursor->x, cursor->lineY);
                        int drawstringKind;
                        if (markStart <= cursor->codepointpos && cursor->codepointpos < markEnd)
                                drawstringKind = DRAWSTRING_HIGHLIGHT;
                        else
                                drawstringKind = DRAWSTRING_NORMAL;
                        uint32_t codepoint = read_codepoint_from_UTF8Decoder(&decoder);

                        /* The text editor should be able to edit any kind of file (even
                         * binary). It should always operate on the literal file contents, not
                         * just some representation of the contents. Convenience should be
                         * provided by views, i.e. standard editing view would be a UTF-8
                         * presentation where \r are either always displayed (Unix line endings
                         * mode) or only displayed when not followed by a \n character (DOS
                         * mode). Another view would be "binary" where all characters are always
                         * visible (displayed as hex, for example).
                         */
                        /* For now, we just implement a standard text editor view */
                        if (codepoint == '\n') {
                                cursor->codepointpos++;
                                cursor->lineNumber++;
                                next_line(cursor);
                        }
                        else if (codepoint == '\r') {
                                // TODO: display as \r (literally)
                                draw_codepoint(cursor, box, drawstringKind, 0x23CE);  /* Unicode 'RETURN SYMBOL' */
                        }
                        else {
                                draw_codepoint(cursor, box, drawstringKind, codepoint);
                        }
                }

                if (token.tokenKind == BLUNT_TOKEN_EOF)
                        break;
        }
        // markStart/markEnd are currently abused to draw the text cursor, and
        // in this case here we know it has to be the text cursor
        if (readpos_in_bytes_of_UTF8Decoder(&decoder) == edit->cursorBytePosition)
                draw_cursor(edit, cursor->x, cursor->lineY);
        end_lexing_blunt_tokens(&readCtx);
        exit_UTF8Decoder(&decoder);
}

static void draw_textedit_ViCmdline(struct TextEdit *edit, int x, int y, int w, int h)
{
        draw_colored_rect(x, y, w, h, C(statusbarBgColor));

        struct GuiRect boundingBox;
        struct DrawCursor drawCursor;
        struct GuiRect *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, 0, 0, windowWidthInPixels, windowHeightInPixels);
        set_draw_cursor(cursor, x, y, 0, 0);
        set_cursor_color(cursor, C(statusbarTextColor));

        draw_text_with_cursor(cursor, box, ":", 1);

        const char *text;
        int textLength;
        int cursorBytePosition;
        if (edit->vistate.cmdline.isNavigatingHistory) {
                struct CmdlineHistory *history = &edit->vistate.cmdline.history;
                text = history->iter->cmdline;
                textLength = history->iter->length;
                cursorBytePosition = history->iter->length; //XXX???
        }
        else {
                text = edit->vistate.cmdline.buf;
                textLength = edit->vistate.cmdline.fill;
                cursorBytePosition = edit->vistate.cmdline.cursorBytePosition;
        }

        struct FixedStringUTF8Decoder decoder = {text, textLength};
        for (;;) {
                if (decoder.pos == cursorBytePosition)
                        draw_cursor(edit, cursor->x, cursor->lineY);
                uint32_t codepoint;
                if (!decode_codepoint_from_FixedStringUTF8Decoder(&decoder, &codepoint))
                        //XXX: what if there are only decode errors? Should we
                        //try to recover?
                        break;
                draw_codepoint(cursor, box, DRAWSTRING_NORMAL, codepoint);
        }
}

static void draw_textedit_statusline(struct TextEdit *edit, int x, int y, int w, int h)
{
        flush_all_vertex_buffers();

        FILEPOS pos = edit->cursorBytePosition;
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, pos);
        FILEPOS lineNumber = compute_line_number(edit->rope, pos);
        char textbuffer[512];

        draw_colored_rect(x, y, w, h, C(statusbarBgColor));

        struct GuiRect boundingBox;
        struct DrawCursor drawCursor;
        struct GuiRect *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, x, y, w, h);
        set_draw_cursor(cursor, x, y, 0, 0);
        set_cursor_color(cursor, C(statusbarTextColor));

        if (edit->haveNotification) {
                if (edit->notificationKind == NOTIFICATION_ERROR)
                        set_cursor_color(cursor, 255, 0, 0, 255);
                draw_text_with_cursor(cursor, box, edit->notificationBuffer, edit->notificationLength);
        }
        else {
                if (edit->isVimodeActive) {
                        if (edit->vistate.vimodeKind != VIMODE_NORMAL)
                                draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, "VI MODE: -- %s --", vimodeKindString[edit->vistate.vimodeKind]);
                }
                set_draw_cursor(cursor, x + 500, y, 0, 0);
                draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, " pos: %"FILEPOS_PRI, pos);
                draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", codepointPos: %"FILEPOS_PRI, codepointPos);
                draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", lineNumber: %"FILEPOS_PRI, lineNumber);
                draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", selecting?: %d", edit->isSelectionMode);
        }
}

static void draw_textedit_loading_or_saving(const char *what, FILEPOS count, FILEPOS total, int x, int y, int w, int h)
{
        flush_all_vertex_buffers();

        struct GuiRect boundingBox;
        struct DrawCursor drawCursor;
        struct GuiRect *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, x, y, w, h);
        set_draw_cursor(cursor, x, y, 0, 0);

        char textbuffer[512];

        draw_colored_rect(x, y, w, h, C(statusbarBgColor));

        /* XXX: this computation is very dirty */
        FILEPOS percentage = (FILEPOS) (filepos_mul(count, 100) / (total ? total : 1));

        set_cursor_color(cursor, C(statusbarTextColor));
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer,
                "%s %"FILEPOS_PRI"%%  (%"FILEPOS_PRI" / %"FILEPOS_PRI" bytes)",
                what, percentage, count, total);
}

static void draw_textedit_loading(struct TextEdit *edit, int x, int y, int w, int h)
{
        FILEPOS count = edit->loading.completedBytes;
        FILEPOS total = edit->loading.totalBytes;

        draw_textedit_loading_or_saving("Loading", count, total, x, y, w, h);
}

static void draw_textedit_saving(struct TextEdit *edit, int x, int y, int w, int h)
{
        FILEPOS count = edit->saving.completedBytes;
        FILEPOS total = edit->saving.totalBytes;

        draw_textedit_loading_or_saving("Saving", count, total, x, y, w, h);
}

static void draw_TextEdit(int canvasX, int canvasY, int canvasW, int canvasH, struct TextEdit *edit)
{
        // clear
        draw_colored_rect(0, 0, windowWidthInPixels, windowHeightInPixels, C(texteditBgColor));

        int statusLineH = 40;
        int statusLineX = canvasX;
        int statusLineY = canvasH - statusLineH; if (statusLineY < 0) statusLineY = 0;
        int statusLineW = canvasW;

        if (edit->loading.isActive) {
                draw_textedit_loading(edit, statusLineX, statusLineY, statusLineW, statusLineH);
                return;
        }
        if (edit->saving.isActive) {
                draw_textedit_saving(edit, statusLineX, statusLineY, statusLineW, statusLineH);
                return;
        }

        int restH = canvasH - statusLineH;
        if (restH < 0)
                restH = 0;

        // XXX is here the right place to do this?
        edit->numberOfLinesDisplayed = restH / TEXT_HEIGHT_PIXELS;

        /* Compute first line and y-offset for drawing */
        FILEPOS firstVisibleLine;
        int offsetPixelsY;
        if (edit->scrollAnimation.isActive) {
                //XXX overflow?
                float linesProgress = edit->scrollAnimation.progress * (edit->scrollAnimation.targetLine - edit->scrollAnimation.startLine);
                firstVisibleLine = edit->scrollAnimation.startLine + (FILEPOS) linesProgress;
                offsetPixelsY = (int) (((linesProgress) - (FILEPOS) linesProgress) * TEXT_HEIGHT_PIXELS);
        }
        else {
                firstVisibleLine = edit->firstLineDisplayed;
                offsetPixelsY = 0;
        }

        int linesX = canvasX;
        int linesY = canvasY;
        int linesW = 0;
        int linesH = restH;
        if (globalData.isShowingLineNumbers) {
                //FILEPOS x = firstVisibleLine + edit->numberOfLinesDisplayed;
                FILEPOS x = textrope_number_of_lines_quirky(edit->rope);
                int numDigitsNeeded = 1;
                while (x >= 10) {
                        x /= 10;
                        numDigitsNeeded ++;
                }
                /* allocate space for at least 4 digits to avoid popping in the
                 * normal case. */
                if (numDigitsNeeded < 4)
                        numDigitsNeeded = 4;
                linesW = 30 + numDigitsNeeded * 20; //XXX
        }

        int textAreaX = linesX + linesW;
        int textAreaY = canvasY;
        int textAreaW = canvasW - linesW; if (textAreaW < 0) textAreaW = 0;
        int textAreaH = restH;

        if (globalData.isShowingLineNumbers) {
                draw_line_numbers(edit, firstVisibleLine, offsetPixelsY, linesX, linesY, linesW, linesH);
                draw_vertical_line(textAreaX, textAreaY + 3, textAreaH - 2 * 3);
        }
        draw_textedit_lines(edit, firstVisibleLine, offsetPixelsY, textAreaX, textAreaY, textAreaW, textAreaH);

        if (edit->vistate.vimodeKind == VIMODE_COMMAND)
                draw_textedit_ViCmdline(edit, statusLineX, statusLineY, statusLineW, statusLineH);
        else
                draw_textedit_statusline(edit, statusLineX, statusLineY, statusLineW, statusLineH);
}


#include <astedit/buffers.h>
void draw_buffer_list(int canvasX, int canvasY, int canvasW, int canvasH)
{
        struct DrawCursor drawCursor;
        struct GuiRect boundingBox;

        struct DrawCursor *cursor = &drawCursor;
        struct GuiRect *box = &boundingBox;

        int bufferBoxX = 20;
        int bufferBoxY = 20;
        int bufferBoxW = canvasW - 20 - (bufferBoxX - canvasX);
        int bufferBoxH = 40;

        set_draw_cursor(cursor, bufferBoxX, bufferBoxY, 0, 0);
        set_cursor_color(cursor, C(normalTextColor));
        set_bounding_box(box, canvasX, canvasY, canvasW, canvasH);

        // actually, override this stuff
        cursor->lineHeight = bufferBoxH;
        cursor->distanceYtoBaseline = bufferBoxH * 2 / 3;

        draw_colored_rect(canvasX, canvasY, canvasW, canvasH, C(texteditBgColor));

        for (struct Buffer *buffer = buffers;
                buffer != NULL;
                buffer = buffer->next)
        {
                int borderX = bufferBoxX;
                int borderY = cursor->lineY;
                int borderH = cursor->lineHeight;
                int borderW = bufferBoxW;
                int borderThickness = 1;

                struct RGB rgb;
                if (globalData.selectedBuffer == buffer)
                        rgb = (struct RGB) { 255, 0, 0 };
                else
                        rgb = currentLineBorderColor;

                // TODO: make sure that the outline for the selected item is
                // fully visible
                draw_colored_border(borderX, borderY, borderW, borderH,
                                    borderThickness, C(rgb));

                const char *name = buffer->name;
                int nameLength = (int) strlen(name);
                draw_text_with_cursor(cursor, box, name, nameLength);

                next_line(cursor);
        }
}

void testdraw(struct TextEdit *edit)
{
        int canvasX = 0;
        int canvasY = 0;
        int canvasW = windowWidthInPixels;
        int canvasH = windowHeightInPixels;

        if (canvasW < 0)
                canvasW = 0;
        if (canvasH < 0)
                canvasH = 0;

        clear_screen_and_drawing_state();
        begin_frame(canvasX, canvasY, canvasW, canvasH);

        if (globalData.isSelectingBuffer)
                draw_buffer_list(0, 0, canvasW, canvasH);
        else
                draw_TextEdit(0, 0, canvasW, canvasH, edit);


        end_frame();
}
