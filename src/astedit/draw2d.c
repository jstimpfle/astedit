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
        uint32_t codepoint, int r, int g, int b, int a)
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
                cursor->x, cursor->y, r, g, b, a);
        if (drawstringKind == DRAWSTRING_HIGHLIGHT)
                draw_colored_rect(cursor->x, cursor->y - cursor->ascender,
                        xEnd - cursor->x, cursor->lineHeight, 0, 0, 192, 32);
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
                        draw_codepoint(cursor, boundingBox, DRAWSTRING_NORMAL, codepoints[j], 0, 0, 0, 255);
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
static void set_draw_cursor(struct DrawCursor *cursor, int x, int y, int codepointPos, int lineNumber)
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


static void draw_line_numbers(struct TextEdit *edit, int firstLine, int numberOfLines, int x, int y, int w, int h)
{
        UNUSED(edit);

        struct BoundingBox boundingBox;
        struct DrawCursor drawCursor;
        struct BoundingBox *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, x, y, w, h);
        set_draw_cursor(cursor, x, y, 0, 0);

        int onePastLastLine = firstLine + numberOfLines;

        for (int i = firstLine; i < onePastLastLine; i++) {
                char buf[16];
                snprintf(buf, sizeof buf, "%4d", i + 1);
                draw_text_with_cursor(cursor, &boundingBox, buf, (int) strlen(buf));
                next_line(cursor);
        }
}

static void draw_textedit_lines(struct TextEdit *edit, int firstLine, int numberOfLines,
        int x, int y, int w, int h, int markStart, int markEnd)
{
        flush_all_vertex_buffers();

        int initialReadPos = compute_pos_of_line(edit->rope, firstLine);
        int initialCodepointPos = compute_codepoint_position(edit->rope, initialReadPos);

        struct BoundingBox boundingBox;
        struct DrawCursor drawCursor;
        struct BoundingBox *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        set_bounding_box(box, x, y, w, h);
        set_draw_cursor(cursor, x, y, initialCodepointPos, firstLine);

        struct TextropeUTF8Decoder decoder;
        init_UTF8Decoder(&decoder, edit->rope, initialReadPos);

        int onePastLastLine = firstLine + numberOfLines;

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

                int currentPos = readpos_in_bytes_of_UTF8Decoder(&decoder);
                int whiteEndPos = currentPos + token.leadingWhiteChars;
                int tokenEndPos = currentPos + token.length;
                while (readpos_in_bytes_of_UTF8Decoder(&decoder) < whiteEndPos) {
                        uint32_t codepoint = read_codepoint_from_UTF8Decoder(&decoder);
                        draw_codepoint(cursor, box, DRAWSTRING_NORMAL, codepoint, 0, 0, 0, 255);
                }
                while (readpos_in_bytes_of_UTF8Decoder(&decoder) < tokenEndPos) {
                        uint32_t codepoint = read_codepoint_from_UTF8Decoder(&decoder);
                        int drawstringKind = DRAWSTRING_NORMAL;
                        if (markStart <= cursor->codepointpos && cursor->codepointpos < markEnd)
                                drawstringKind = DRAWSTRING_HIGHLIGHT;
                        int r = 0, g = 0, b = 0, a = 255;
                        if (token.tokenKind == BLUNT_TOKEN_INTEGER) {
                                r = 0; g = 0; b = 255; a = 255;
                        }
                        else if (token.tokenKind == BLUNT_TOKEN_STRING) {
                                r = 0; g = 255; b = 0; a = 255;
                        }
                        else if (FIRST_BLUNT_TOKEN_OPERATOR <= token.tokenKind
                                && token.tokenKind <= LAST_BLUNT_TOKEN_OPERATOR) {
                                r = 0; g = 0; b = 0; a = 255;
                        }
                        else if (token.tokenKind == BLUNT_TOKEN_JUNK) {
                                r = 255; g = 0; b = 0; a = 255;
                        }
                        draw_codepoint(cursor, box, drawstringKind, codepoint, r, g, b, a);
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

        struct BoundingBox boundingBox;
        struct DrawCursor drawCursor;
        struct BoundingBox *box = &boundingBox;
        struct DrawCursor *cursor = &drawCursor;

        // no actual bounding box currently.
        set_bounding_box(box, 0, 0, windowWidthInPixels, windowHeightInPixels);
        set_draw_cursor(cursor, x, y, 0, 0);

        int pos = edit->cursorBytePosition;
        int codepointPos = compute_codepoint_position(edit->rope, pos);
        int lineNumber = compute_line_number(edit->rope, pos);
        char textbuffer[512];

        draw_colored_rect(x, y, w, h, 128, 160, 128, 255);

        if (edit->isVimodeActive)
                draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, "VI MODE: -- %s --", vimodeKindString[edit->vistate.vimodeKind]);

        cursor->x = x + 500;
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, " pos: %d", pos);
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", codepointPos: %d", codepointPos);
        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer, ", lineNumber: %d", lineNumber);
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

        draw_colored_rect(x, y, w, h, 128, 160, 128, 255);

        draw_text_snprintf(cursor, box, textbuffer, sizeof textbuffer,
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
        int canvasW = windowWidthInPixels - 200;
        int canvasH = windowHeightInPixels - 200;

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
