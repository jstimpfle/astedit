#include <astedit/astedit.h>
#include <astedit/bytes.h>
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

static const struct RGB texteditBgColor = { 0, 0, 0 };
static const struct RGB statusbarBgColor = { 128, 160, 128 };
static const struct RGB normalTextColor = {0, 255, 0 };
static const struct RGB stringTokenColor = { 0, 255, 0 };
static const struct RGB integerTokenColor = { 0, 0, 255 };
static const struct RGB operatorTokenColor = {0, 0, 255};  // same as normalTextColor, but MSVC won't let me do that ("initializer is not a constant")
static const struct RGB junkTokenColor = { 255, 0, 0 };
static const struct RGB borderColor = { 32, 32, 32 };
static const struct RGB highlightColor = { 0, 0, 255 };
static const struct RGB statusbarTextColor = { 0, 0, 0 };

static struct ColorVertex2d colorVertexBuffer[3 * 1024];
static struct TextureVertex2d alphaVertexBuffer[3 * 1024];

static int colorVertexCount;
static int alphaVertexCount;


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
                int n = LENGTH(colorVertexBuffer) - colorVertexCount;
                if (n > length - i)
                        n = length - i;
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

// TODO find better and useful concepts, e.g. begin_region() / end_region()?
static void flush_all_vertex_buffers(void)
{
        flush_color_vertices();
        flush_alpha_texture_vertices();
        //flush_rgba_texture_vertices();
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
        flush_all_vertex_buffers();
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


static void draw_codepoint(
        struct DrawCursor *cursor,
        const struct BoundingBox *boundingBox,
        int drawstringKind,
        uint32_t codepoint)
{
        cursor->codepointpos++;
        if (codepoint == '\r')
                return;
        if (codepoint == '\n') {
                next_line(cursor);
                cursor->lineNumber++;
                return;
        }
        int xEnd = draw_glyphs_on_baseline(FONTFACE_REGULAR, boundingBox,
                cursor->fontSize, &codepoint, 1,
                cursor->x, cursor->y,
                cursor->r, cursor->g, cursor->b, cursor->a);
        if (drawstringKind == DRAWSTRING_HIGHLIGHT)
                draw_colored_rect(cursor->x, cursor->y - cursor->ascender,
                        xEnd - cursor->x, cursor->lineHeight, C(highlightColor));
        cursor->x = xEnd;
}

static void draw_text_with_cursor(
        struct DrawCursor *cursor,
        const struct BoundingBox *boundingBox,
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
        const struct BoundingBox *boundingBox,
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

static void set_bounding_box(struct BoundingBox *box, int x, int y, int w, int h)
{
        box->bbX = x;
        box->bbY = y;
        box->bbW = w;
        box->bbH = h;
}


static const int LINE_HEIGHT = 33;
static void set_draw_cursor(struct DrawCursor *cursor, int x, int y, FILEPOS codepointPos, FILEPOS lineNumber)
{
        cursor->xLeft = x;
        cursor->fontSize = 25;
        cursor->ascender = 20;
        cursor->lineHeight = LINE_HEIGHT;
        cursor->x = x;
        cursor->y = y + LINE_HEIGHT;
        cursor->codepointpos = codepointPos;
        cursor->lineNumber = lineNumber;
}

static void set_cursor_color(struct DrawCursor *cursor, int r, int g, int b, int a)
{
        cursor->r = r;
        cursor->g = g;
        cursor->b = b;
        cursor->a = a;
}


static void draw_line_numbers(struct TextEdit *edit, FILEPOS firstLine, FILEPOS numberOfLines, int x, int y, int w, int h)
{
        UNUSED(edit);

        struct BoundingBox boundingBox;
        struct DrawCursor drawCursor;
        struct BoundingBox *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, x, y, w, h);
        set_draw_cursor(cursor, x, y, 0, 0);

        FILEPOS onePastLastLine = filepos_add(firstLine, numberOfLines);

        set_cursor_color(cursor, C(normalTextColor));
        for (FILEPOS i = firstLine; i < onePastLastLine; i++) {
                char buf[16];
                snprintf(buf, sizeof buf, "%4"FILEPOS_PRI, i + 1);
                draw_text_with_cursor(cursor, &boundingBox, buf, (int) strlen(buf));
                next_line(cursor);
        }

        {
                int pad = 10;
                int bx = x + w;
                int by = y + pad;
                int bw = 2;
                int bh = y + h - pad;
                if (bh < by)
                        bh = by;
                draw_colored_rect(bx, by, bw, bh, C(borderColor));
        }
}

static void draw_textedit_lines(struct TextEdit *edit, FILEPOS firstLine, FILEPOS numberOfLines,
        int x, int y, int w, int h, FILEPOS markStart, FILEPOS markEnd)
{
        flush_all_vertex_buffers();

        FILEPOS initialReadPos = compute_pos_of_line(edit->rope, firstLine);
        FILEPOS initialCodepointPos = compute_codepoint_position(edit->rope, initialReadPos);

        struct BoundingBox boundingBox;
        struct DrawCursor drawCursor;
        struct BoundingBox *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, x, y, w, h);
        set_draw_cursor(cursor, x, y, initialCodepointPos, firstLine);

        struct TextropeUTF8Decoder decoder;
        init_UTF8Decoder(&decoder, edit->rope, initialReadPos);

        FILEPOS onePastLastLine = filepos_add(firstLine, numberOfLines);

        struct Blunt_ReadCtx readCtx;
        struct Blunt_Token token;
        begin_lexing_blunt_tokens(&readCtx, edit->rope, initialReadPos);

        /*
        We can start lexing at the first character of the line
        only because the lexical syntax is made such that newline
        is always a token separator. Otherwise, we'd need to find
        the start a little before that line and with more complicated
        code.
        */

        while (cursor->lineNumber < onePastLastLine) {
                lex_blunt_token(&readCtx, &token);

                FILEPOS currentPos = readpos_in_bytes_of_UTF8Decoder(&decoder);
                FILEPOS whiteEndPos = currentPos + token.leadingWhiteChars;
                FILEPOS tokenEndPos = currentPos + token.length;
                set_cursor_color(cursor, C(normalTextColor));
                while (readpos_in_bytes_of_UTF8Decoder(&decoder) < tokenEndPos) {
                        uint32_t codepoint = read_codepoint_from_UTF8Decoder(&decoder);
                        int drawstringKind = DRAWSTRING_NORMAL;
                        if (markStart <= cursor->codepointpos && cursor->codepointpos < markEnd)
                                drawstringKind = DRAWSTRING_HIGHLIGHT;
                        struct RGB rgb;
                        if (token.tokenKind == BLUNT_TOKEN_INTEGER)
                                rgb = integerTokenColor;
                        else if (token.tokenKind == BLUNT_TOKEN_STRING)
                                rgb = stringTokenColor;
                        else if (FIRST_BLUNT_TOKEN_OPERATOR <= token.tokenKind
                                && token.tokenKind <= LAST_BLUNT_TOKEN_OPERATOR)
                                rgb = operatorTokenColor;
                        else if (token.tokenKind == BLUNT_TOKEN_JUNK)
                                rgb = junkTokenColor;
                        else
                                rgb = normalTextColor;
                        set_cursor_color(cursor, C(rgb));
                        draw_codepoint(cursor, box, drawstringKind, codepoint);
                }

                if (token.tokenKind == BLUNT_TOKEN_EOF)
                        break;
        }
        end_lexing_blunt_tokens(&readCtx);
        exit_UTF8Decoder(&decoder);
}

static void draw_textedit_statusline(struct TextEdit *edit, int x, int y, int w, int h)
{
        flush_all_vertex_buffers();

        FILEPOS pos = edit->cursorBytePosition;
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, pos);
        FILEPOS lineNumber = compute_line_number(edit->rope, pos);
        char textbuffer[512];

        draw_colored_rect(x, y, w, h, C(statusbarBgColor));

        struct BoundingBox boundingBox;
        struct DrawCursor drawCursor;
        struct BoundingBox *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        // no actual bounding box currently.
        set_bounding_box(box, 0, 0, windowWidthInPixels, windowHeightInPixels);
        set_draw_cursor(cursor, x, y, 0, 0);
        set_cursor_color(cursor, C(statusbarTextColor));

        if (edit->isVimodeActive)
                draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, "VI MODE: -- %s --", vimodeKindString[edit->vistate.vimodeKind]);

        set_draw_cursor(cursor, x + 500, y, 0, 0);
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, " pos: %"FILEPOS_PRI, pos);
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", codepointPos: %"FILEPOS_PRI, codepointPos);
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", lineNumber: %"FILEPOS_PRI, lineNumber);
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", selecting?: %d", edit->isSelectionMode);
}

static void draw_textedit_loading(struct TextEdit *edit, int x, int y, int w, int h)
{
        flush_all_vertex_buffers();

        // no actual bounding box currently.
        struct BoundingBox boundingBox;
        struct DrawCursor drawCursor;
        struct BoundingBox *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, 0, 0, windowWidthInPixels, windowHeightInPixels);
        set_draw_cursor(cursor, x, y, 0, 0);

        char textbuffer[512];

        draw_colored_rect(x, y, w, h, C(statusbarBgColor));

        /* XXX: this computation is very dirty */
        FILEPOS percentage = (FILEPOS) (filepos_mul(edit->loadingCompletedBytes, 100)
                          / (edit->loadingTotalBytes ? edit->loadingTotalBytes : 1));

        set_cursor_color(cursor, C(statusbarTextColor));
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer,
                "Loading %"FILEPOS_PRI"%%  (%"FILEPOS_PRI" / %"FILEPOS_PRI" bytes)",
                percentage, edit->loadingCompletedBytes, edit->loadingTotalBytes);
}


static void draw_TextEdit(int canvasX, int canvasY, int canvasW, int canvasH,
        struct TextEdit *edit, FILEPOS firstLine, FILEPOS markStart, FILEPOS markEnd)
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

        draw_colored_rect(0, 0, windowWidthInPixels, windowHeightInPixels, C(texteditBgColor));

        if (edit->isLoading) {
                draw_textedit_loading(edit, statusLineX, statusLineY, statusLineW, statusLineH);
        } else {
                FILEPOS length = textrope_length(edit->rope);

                if (markStart < 0) markStart = 0;
                if (markEnd < 0) markEnd = 0;
                if (markStart >= length) markStart = length;
                if (markEnd >= length) markEnd = length;

                ENSURE(markStart <= markEnd);

                FILEPOS maxNumberOfLines = (textAreaH + LINE_HEIGHT - 1) / LINE_HEIGHT;
                FILEPOS numberOfLines = textrope_number_of_lines_quirky(edit->rope) - firstLine;
                if (numberOfLines > maxNumberOfLines)
                        numberOfLines = maxNumberOfLines;

                // XXX is here the right place to do this?
                edit->numberOfLinesDisplayed = textAreaH / LINE_HEIGHT;

                draw_line_numbers(edit, firstLine, numberOfLines, linesX, linesY, linesW, linesH);
                draw_textedit_lines(edit, firstLine, numberOfLines, textAreaX, textAreaY, textAreaW, textAreaH, markStart, markEnd);
                draw_textedit_statusline(edit, statusLineX, statusLineY, statusLineW, statusLineH);
        }
}

void testdraw(struct TextEdit *edit)
{
        FILEPOS markStart;
        FILEPOS markEnd;
        if (edit->isSelectionMode)
                get_selected_range_in_codepoints(edit, &markStart, &markEnd);
        else {
                markStart = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
                markEnd = markStart + 1;
        }

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

        draw_TextEdit(0, 0, canvasW, canvasH,
                edit, edit->firstLineDisplayed,
                markStart, markEnd);

        end_frame();
}
