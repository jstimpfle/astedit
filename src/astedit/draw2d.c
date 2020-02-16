#include <astedit/astedit.h>
#include <astedit/memory.h>
#include <astedit/bytes.h>
#include <astedit/editor.h>
#include <astedit/logging.h>
#include <astedit/listselect.h>
#include <astedit/window.h>
#include <astedit/font.h>
#include <astedit/gfx.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>
#include <astedit/utf8.h>
#include <astedit/textpositions.h>
#include <astedit/textropeUTF8decode.h>
#include <astedit/zoom.h>
#include <astedit/draw2d.h>
#include <blunt/lex.h>
#include <stdio.h> // snprintf
#include <string.h> // strlen()

struct DrawCursor {
        int xLeft;
        int fontSize;
        int distanceYtoBaseline;
        int lineHeight;
        /* We have switched to monospace.
         * That considerably simplifies calculations.
         * Currently there's no support for non-monospace. */
        int cellWidth;
        /**/
        int x;
        int lineY;
        int r;
        int g;
        int b;
        int a;
};

struct DrawList {
        int mustRelayout;

        struct LayedOutGlyph *glyphs;
        struct LayedOutRect *rects;

        int glyphsCount;
        int rectsCount;
};


struct RGB { unsigned r, g, b; };
#define C(x) x.r, x.g, x.b, 255
#define C_ALPHA(x, a) x.r, x.g, x.b, a


#if 1
static const struct RGB texteditBgColor = { 0, 0, 0 };
static const struct RGB statusbarBgColor = { 128, 160, 128 };
static const struct RGB normalTextColor = {255, 255, 255 };
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


void next_line(struct DrawCursor *cursor)
{
        cursor->x = cursor->xLeft;
        cursor->lineY += cursor->lineHeight;
}

static struct DrawList contentsDrawList;
static struct DrawList numbersDrawList;
static struct DrawList statuslineDrawList;
static struct DrawList windowDrawList;


static int linesX;
static int linesY;
static int linesW;
static int linesH;

static int textAreaX;
static int textAreaY;
static int textAreaW;
static int textAreaH;

static int statuslineX;
static int statuslineY;
static int statuslineW;
static int statuslineH;

static struct LayedOutGlyph *alloc_LayedOutGlyph(struct DrawList *drawList)
{
        int idx = drawList->glyphsCount++;
        REALLOC_MEMORY(&drawList->glyphs, drawList->glyphsCount);
        return drawList->glyphs + idx;
}

static struct LayedOutRect *alloc_LayedOutRect(struct DrawList *drawList, int n)
{
        int idx = drawList->rectsCount;
        drawList->rectsCount += n;
        REALLOC_MEMORY(&drawList->rects, drawList->rectsCount);
        return drawList->rects + idx;
}

static void clear_DrawList(struct DrawList *drawList)
{
        drawList->glyphsCount = 0;
        drawList->rectsCount = 0;
}

static void lay_out_rect(struct DrawList *drawList, int x, int y, int w, int h,
                       int r, int g, int b, int a)
{
        alloc_LayedOutRect(drawList, 1)[0] = (struct LayedOutRect) { x, y, w, h, r, g, b, a };
}

static void lay_out_border(struct LayedOutRect border[4],
                 int x, int y, int w, int h, int t,
                 int r, int g, int b, int a)
{
        border[0] = (struct LayedOutRect) { x, y, w, t,          r, g, b, a };
        border[1] = (struct LayedOutRect) { x, y + h - t, w, t,  r, g, b, a };
        border[2] = (struct LayedOutRect) { x, y, t, h,          r, g, b, a };
        border[3] = (struct LayedOutRect) { x + w - t, y, t, h,  r, g, b, a };
}

static void add_and_lay_out_border(struct DrawList *drawList,
                                   int x, int y, int w, int h, int t,
                                   int r, int g, int b, int a)
{
        struct LayedOutRect *border = alloc_LayedOutRect(drawList, 4);
        lay_out_border(border, x, y, w, h, t, r, g, b, a);
}

static void lay_out_glyph(
        struct DrawList *drawList,
        struct DrawCursor *cursor,
        uint32_t codepoint)
{
        struct TexDrawInfo tdi;
        get_TexDrawInfo_for_glyph(FONTFACE_REGULAR, cursor->fontSize, codepoint, &tdi);
        int rectX = cursor->x + tdi.bearingX;
        int rectY = cursor->lineY + cursor->distanceYtoBaseline - tdi.bearingY;

        struct LayedOutGlyph *log = alloc_LayedOutGlyph(drawList);
        log->tex = tdi.tex;
        log->tx = tdi.texX;
        log->ty = tdi.texY;
        log->tw = tdi.texW;
        log->th = tdi.texH;
        log->x = rectX;
        log->y = rectY;
        log->r = cursor->r;
        log->g = cursor->g;
        log->b = cursor->b;
        log->a = cursor->a;
        /*
        log_postf("tex: %d, tx=%d, ty=%d, tw=%d, th=%d, x=%d, y=%d",
                  tdi.tex, tdi.texX, tdi.texY, tdi.texW, tdi.texH,
                  log->x, log->y);
                  */

        cursor->x += cellWidthPx;
        //cursor->x += tdi.horiAdvance;
}

static void lay_out_text_with_cursor(
        struct DrawList *drawList,
        struct DrawCursor *cursor,
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

                for (int j = 0; j < numCodepointsDecoded; j++) {
                        lay_out_glyph(drawList, cursor, codepoints[j]);
                }
        }
}

static void lay_out_text_snprintf(
        struct DrawList *drawList,
        struct DrawCursor *cursor,
        char *buffer, int length,
        const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        int numBytes = vsnprintf(buffer, length, fmt, ap);
        if (numBytes >= 0) {
                //log_postf("laying out at %d,%d: %s", cursor->x, cursor->lineY, buffer);
                lay_out_text_with_cursor(drawList, cursor, buffer, numBytes);
        }
        va_end(ap);
}

static void set_draw_cursor(struct DrawCursor *cursor, int x, int y)
{
        cursor->xLeft = x;
        cursor->fontSize = textHeightPx;
        cursor->distanceYtoBaseline = lineHeightPx * 2 / 3;
        cursor->lineHeight = lineHeightPx;
        cursor->cellWidth = cellWidthPx;
        cursor->x = x;
        cursor->lineY = y;
}

static void set_cursor_color(struct DrawCursor *cursor, int r, int g, int b, int a)
{
        cursor->r = r;
        cursor->g = g;
        cursor->b = b;
        cursor->a = a;
}

static void lay_out_line_numbers(struct DrawList *drawList,
                                 struct TextEdit *edit, FILEPOS firstVisibleLine,
                                 int offsetPixelsY, int w, int h)
{
        lay_out_rect(drawList, 0, 0, w, h, C(texteditBgColor));

        FILEPOS lastLine = textrope_number_of_lines_quirky(edit->rope);

        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;

        set_draw_cursor(cursor, 0, -offsetPixelsY);
        set_cursor_color(cursor, C(normalTextColor));

        for (FILEPOS i = firstVisibleLine;
             i < lastLine && cursor->lineY < h;
             i++) {
                char buf[32];
                snprintf(buf, sizeof buf, "%4"FILEPOS_PRI, i + 1);
                lay_out_text_with_cursor(drawList, cursor, buf, (int) strlen(buf));
                next_line(cursor);
        }

        struct LayedOutRect *r = alloc_LayedOutRect(drawList, 1);
        *r = (struct LayedOutRect) { w - borderWidthPx, 0, borderWidthPx, h,
                                        C(borderColor) };
}

static void lay_out_cursor_active(struct DrawList *drawList, int isActive, int x, int y)
{
        int w = borderWidthPx + (isActive ? 1 : 0);
        int h = lineHeightPx;
        struct LayedOutRect *rect = alloc_LayedOutRect(drawList, 1);
        *rect = (struct LayedOutRect) { x, y, w, h, C(cursorColor) };
}

static void lay_out_cursor(struct DrawList *drawList, struct TextEdit *edit, int x, int y)
{
        int isActive = edit->vistate.vimodeKind == VIMODE_INPUT;
        lay_out_cursor_active(drawList, isActive, x, y);
}

static void lay_out_textedit_lines(
        struct DrawList *drawList,
        struct TextEdit *edit,
        FILEPOS firstVisibleLine, int offsetPixelsY,
        int areaW, int areaH)
{
        lay_out_rect(drawList, 0, 0, areaW, areaH, C(texteditBgColor));

        int x = 5;
        int y = 5;
        int w = areaW - 10;
        //int h = areaH - 10;

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
                init_UTF8Decoder(&decoder, edit->rope, initialReadPos);
                begin_lexing_blunt_tokens(&readCtx, edit->rope, initialReadPos);
                set_draw_cursor(cursor, x, y - offsetPixelsY);
        }

        // AAARGH, we shouldn't call the drawing context "cursor"
        FILEPOS markStart = 0;
        FILEPOS markEnd = 0;
        {
                FILEPOS textropeLength = textrope_length(edit->rope);
                if (edit->isSelectionMode)
                        get_selected_range_in_bytes(edit, &markStart, &markEnd);
                if (markStart < 0) markStart = 0;
                if (markEnd < 0) markEnd = 0;
                if (markStart >= textropeLength) markStart = textropeLength;
                if (markEnd >= textropeLength) markEnd = textropeLength;
        }
        ENSURE(markStart <= markEnd);

        { // draw box of line width where the cursor is.
                FILEPOS currentLine = compute_line_number(edit->rope, edit->cursorBytePosition);
                int linesDiff = cast_filepos_to_int(currentLine - firstVisibleLine);
                int borderX = x;
                int borderY = cursor->lineY + lineHeightPx * linesDiff;
                int borderH = cursor->lineHeight;
                int borderW = w;
                struct LayedOutRect *border = alloc_LayedOutRect(&contentsDrawList, 6);
                lay_out_border(border, borderX, borderY, borderW, borderH, borderWidthPx,
                               C_ALPHA(currentLineBorderColor, 128));
        }

        while (cursor->lineY < areaH) {
                struct Blunt_Token token;
                lex_blunt_token(&readCtx, &token);
                FILEPOS tokenEndPos = readCtx.readPos;
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
                                lay_out_cursor(drawList, edit, cursor->x, cursor->lineY);
                        if (markStart <= readpos && readpos < markEnd) {
                                // XXX: could encode the shape from markStart to
                                // markEnd more efficiently.
                                lay_out_rect(drawList, cursor->x, cursor->lineY,
                                             cellWidthPx, lineHeightPx,
                                             C(highlightColor));
                        }

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
                        uint32_t codepoint = read_codepoint_from_UTF8Decoder(&decoder);
                        ENSURE(codepoint != -1);  // should only happen at end of stream.
                        if (codepoint == '\n')
                                next_line(cursor);
                        else if (cursor->x >= areaW)
                                break;
                        else if (codepoint == '\r')
                                // TODO: display as \r (literally)
                                lay_out_glyph(&contentsDrawList, cursor, 0x23CE);  /* Unicode 'RETURN SYMBOL' */
                        else
                                lay_out_glyph(&contentsDrawList, cursor, codepoint);
                }
                if (token.tokenKind == BLUNT_TOKEN_EOF)
                        break;
                if (cursor->x >= areaW) {
                        // skip rest of line.
                        struct FileCursor fc = { readpos_in_bytes_of_UTF8Decoder(&decoder) };
                        get_position_line_end(edit, &fc);
                        // because we skipped, need to reset read buffers
                        exit_UTF8Decoder(&decoder);
                        init_UTF8Decoder(&decoder, edit->rope, fc.bytePosition);
                        end_lexing_blunt_tokens(&readCtx);
                        begin_lexing_blunt_tokens(&readCtx, edit->rope, fc.bytePosition);
                }
        }
        // markStart/markEnd are currently abused to draw the text cursor, and
        // in this case here we know it has to be the text cursor
        if (readpos_in_bytes_of_UTF8Decoder(&decoder) == edit->cursorBytePosition)
                lay_out_cursor(drawList, edit, cursor->x, cursor->lineY);
        end_lexing_blunt_tokens(&readCtx);
        exit_UTF8Decoder(&decoder);
}

static void lay_out_textedit_ViCmdline(struct DrawList *drawList, struct TextEdit *edit, int w, int h)
{
        lay_out_rect(drawList, 0, 0, w, h, C(statusbarBgColor));

        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;

        set_draw_cursor(cursor, 0, 0);
        set_cursor_color(cursor, C(statusbarTextColor));

        lay_out_text_with_cursor(drawList, cursor, ":", 1);

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
                        lay_out_cursor(drawList, edit, cursor->x, cursor->lineY);
                uint32_t codepoint;
                if (!decode_codepoint_from_FixedStringUTF8Decoder(&decoder, &codepoint))
                        //XXX: what if there are only decode errors? Should we
                        //try to recover?
                        break;
                lay_out_glyph(&statuslineDrawList, cursor, codepoint);
        }
}

static void lay_out_statusline(struct DrawList *drawList, struct TextEdit *edit, int w, int h)
{
        lay_out_rect(drawList, 0, 0, w, h, C(statusbarBgColor));

        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;

        set_draw_cursor(cursor, 0, 0);
        set_cursor_color(cursor, C(statusbarTextColor));

        if (edit->haveNotification) {
                if (edit->notificationKind == NOTIFICATION_ERROR)
                        set_cursor_color(cursor, 255, 0, 0, 255);
                lay_out_text_with_cursor(drawList, cursor, edit->notificationBuffer, edit->notificationLength);
        }
        else if (!edit->loading.isActive) {  // Important: Not allowed to access the rope while the textrope modified.
                FILEPOS pos = edit->cursorBytePosition;
                FILEPOS codepointPos = compute_codepoint_position(edit->rope, pos);
                FILEPOS lineNumber = compute_line_number(edit->rope, pos);
                FILEPOS posOfLine = compute_pos_of_line(edit->rope, lineNumber);
                FILEPOS codepointPosOfLine = compute_codepoint_position(edit->rope, posOfLine);

                char textbuffer[512];
                if (edit->isVimodeActive) {
                        if (edit->vistate.vimodeKind != VIMODE_NORMAL)
                                lay_out_text_snprintf(drawList, cursor, textbuffer, sizeof textbuffer, "--%s--", vimodeKindString[edit->vistate.vimodeKind]);
                }
                /* TODO: need measuring routines. Or simply first sprintf() to a
                 * buffer manually
                 * As a hack, we're remembering the glyph position where we
                 * added new stuff */
                int firstIndex = drawList->glyphsCount;
                set_draw_cursor(cursor, w - 80 * cellWidthPx, 0);

                lay_out_text_snprintf(drawList, cursor, textbuffer, sizeof textbuffer, "line: %" FILEPOS_PRI, lineNumber);
                lay_out_text_snprintf(drawList, cursor, textbuffer, sizeof textbuffer, ", pos: %" FILEPOS_PRI, pos);
                lay_out_text_snprintf(drawList, cursor, textbuffer, sizeof textbuffer, ", col: %" FILEPOS_PRI "-%" FILEPOS_PRI, codepointPos - codepointPosOfLine, pos - posOfLine);
                lay_out_text_snprintf(drawList, cursor, textbuffer, sizeof textbuffer, ", codepoint: %" FILEPOS_PRI, codepointPos);
                lay_out_text_snprintf(drawList, cursor, textbuffer, sizeof textbuffer, ", selecting?: %d", edit->isSelectionMode);
                int x = w - cellWidthPx;
                for (int i = drawList->glyphsCount; i --> firstIndex;) {
                        x -= cellWidthPx;
                        drawList->glyphs[i].x = x;
                }
        }
}

static void lay_out_textedit_loading_or_saving(struct DrawList *drawList, const char *what,
                                            FILEPOS count, FILEPOS total, int w, int h)
{
        UNUSED(w);
        UNUSED(h);
        char textbuffer[512];
        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;

        set_draw_cursor(cursor, 0, 0);
        /* XXX: this computation is very dirty */
        FILEPOS percentage = (FILEPOS) (filepos_mul(count, 100) / (total ? total : 1));

        set_cursor_color(cursor, C(statusbarTextColor));
        lay_out_text_snprintf(drawList, cursor, textbuffer, sizeof textbuffer,
                "%s %"FILEPOS_PRI"%%  (%"FILEPOS_PRI" / %"FILEPOS_PRI" bytes)",
                what, percentage, count, total);
}

static void lay_out_textedit_loading(struct DrawList *drawList, struct TextEdit *edit, int w, int h)
{
        FILEPOS count = edit->loading.completedBytes;
        FILEPOS total = edit->loading.totalBytes;
        lay_out_textedit_loading_or_saving(drawList, "Loading", count, total, w, h);
}

static void lay_out_textedit_saving(struct DrawList *drawList, struct TextEdit *edit, int w, int h)
{
        FILEPOS count = edit->saving.completedBytes;
        FILEPOS total = edit->saving.totalBytes;
        lay_out_textedit_loading_or_saving(drawList, "Saving", count, total, w, h);
}

static void lay_out_TextEdit(int canvasW, int canvasH, struct TextEdit *edit)
{
        linesX = 0;
        linesW = 0;
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
                linesW = cellWidthPx * (numDigitsNeeded + 1);
        }

        textAreaX = linesX + linesW;
        textAreaW = canvasW - textAreaX; if (textAreaW < 0) textAreaW = 0;

        statuslineX = 0;
        statuslineW = canvasW;

        statuslineH = lineHeightPx;
        statuslineY = canvasH - statuslineH; if (statuslineY < 0) statuslineY = 0;

        textAreaY = 0;
        textAreaH = canvasH - statuslineH;
        if (textAreaH < 0)
                textAreaH = 1;

        linesY = 0;
        linesH = canvasH - statuslineH;
        if (linesH < 0)
                linesH = 0;

        // XXX is here the right place to do this?
        edit->numberOfLinesDisplayed = textAreaH / lineHeightPx;
        edit->numberOfColumnsDisplayed = textAreaH / cellWidthPx;

        /* Compute first line and y-offset for drawing */
        FILEPOS firstVisibleLine;
        int offsetPixelsY;
        if (edit->scrollAnimation.isActive) {
                //XXX overflow?
                float linesProgress = edit->scrollAnimation.progress * (edit->scrollAnimation.targetLine - edit->scrollAnimation.startLine);
                firstVisibleLine = edit->scrollAnimation.startLine + (FILEPOS) linesProgress;
                offsetPixelsY = (int) (((linesProgress) - (FILEPOS) linesProgress) * textHeightPx);
        }
        else {
                firstVisibleLine = edit->firstLineDisplayed;
                offsetPixelsY = 0;
        }


        if (edit->loading.isActive)
                lay_out_textedit_loading(&statuslineDrawList, edit, statuslineW, statuslineH);
        else {
                if (edit->saving.isActive)
                        lay_out_textedit_saving(&statuslineDrawList, edit, statuslineW, statuslineH);
                if (globalData.isShowingLineNumbers)
                        lay_out_line_numbers(&numbersDrawList, edit, firstVisibleLine, offsetPixelsY, linesW, linesH);
                lay_out_textedit_lines(&contentsDrawList, edit, firstVisibleLine, offsetPixelsY, textAreaW, textAreaH);
        }

        if (edit->vistate.vimodeKind == VIMODE_COMMAND)
                lay_out_textedit_ViCmdline(&statuslineDrawList, edit, statuslineW, statuslineH);
        else
                lay_out_statusline(&statuslineDrawList, edit, statuslineW, statuslineH);
}

//XXX this is a duplication of lay_out_textedit_ViCmdline.
// We probably should share code.
static void lay_out_LineEdit(struct DrawList *drawList, struct LineEdit *lineEdit, int x, int y, int w, int h)
{
        lay_out_rect(drawList, x, y, w, h, C(statusbarBgColor));

        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;

        set_draw_cursor(cursor, x, y);
        set_cursor_color(cursor, C(statusbarTextColor));

        const char *text = lineEdit->buf;
        int textLength = lineEdit->fill;
        int cursorBytePosition = lineEdit->cursorBytePosition;

        struct FixedStringUTF8Decoder decoder = {text, textLength};
        for (;;) {
                int active = 1;
                if (decoder.pos == cursorBytePosition)
                        lay_out_cursor_active(drawList, active, cursor->x, cursor->lineY);
                uint32_t codepoint;
                if (!decode_codepoint_from_FixedStringUTF8Decoder(&decoder, &codepoint))
                        //XXX: what if there are only decode errors? Should we
                        //try to recover?
                        break;
                lay_out_glyph(drawList, cursor, codepoint);
        }
}

static void lay_out_ListSelect(
                struct DrawList *drawList,
                struct ListSelect *list,
                int canvasW, int canvasH)
{
        struct DrawCursor drawCursor;
        struct DrawCursor *cursor = &drawCursor;

        int bufferBoxX = 20;
        int bufferBoxY = 100;
        int bufferBoxW = canvasW - 20 - bufferBoxX;
        int bufferBoxH = 40;

        set_draw_cursor(cursor, bufferBoxX, bufferBoxY);
        set_cursor_color(cursor, C(normalTextColor));

        // actually, override this stuff
        cursor->lineHeight = bufferBoxH;
        cursor->distanceYtoBaseline = bufferBoxH * 2 / 3;

        lay_out_rect(drawList, 0, 0, canvasW, canvasH, C(texteditBgColor));

        if (list->isFilterActive) {
                // TODO: layout
                lay_out_LineEdit(&statuslineDrawList, &list->filterLineEdit, 0, 0, 500, 50);

                if (!list->isFilterRegexValid)
                        add_and_lay_out_border(drawList, 0, 0, 500, 50, 2, 255, 0, 0, 255);
        }

        for (int i = 0; i < list->numElems; i++) {
                struct ListSelectElem *elem = &list->elems[i];

                if (list->isFilterActive && list->isFilterRegexValid) {
                        if (!match_regex(&list->filterRegex,
                                         elem->caption, elem->captionLength)) {
                                // TODO: how to hanle
                                continue;
                        }
                }

                int borderX = bufferBoxX;
                int borderY = cursor->lineY;
                int borderH = cursor->lineHeight;
                int borderW = bufferBoxW;

                struct RGB rgb;
                if (i == list->selectedElemIndex)
                        rgb = (struct RGB) { 255, 0, 0 };
                else
                        rgb = currentLineBorderColor;

                // TODO: make sure that the outline for the selected item is
                // fully visible
                add_and_lay_out_border(drawList, borderX, borderY, borderW, borderH, borderWidthPx, C(rgb));

                lay_out_text_with_cursor(drawList, cursor, elem->caption, elem->captionLength);

                next_line(cursor);
        }
}

#include <astedit/buffers.h>
void lay_out_buffer_list(int canvasW, int canvasH)
{
        lay_out_ListSelect(&contentsDrawList/*XXX*/, &globalData.bufferSelect, canvasW, canvasH);
}

static void draw_list(struct DrawList *drawList, int x, int y, int w, int h)
{
        set_viewport_in_pixels(x, y, w, h);
        set_2d_coordinate_system(0, 0, w, h);
        draw_rects(drawList->rects, drawList->rectsCount);
        draw_glyphs(drawList->glyphs, drawList->glyphsCount);
}

void testdraw(struct TextEdit *edit)
{
        clear_DrawList(&contentsDrawList);
        clear_DrawList(&numbersDrawList);
        clear_DrawList(&statuslineDrawList);
        clear_DrawList(&windowDrawList);

        contentsDrawList.mustRelayout = 1;  // XXX for development / testing
        numbersDrawList.mustRelayout = 1;  // XXX for development / testing
        statuslineDrawList.mustRelayout = 1;  // XXX for development / testing
        windowDrawList.mustRelayout = 1;

        int canvasW = windowWidthInPixels;
        int canvasH = windowHeightInPixels;

        if (canvasW < 0)
                canvasW = 0;
        if (canvasH < 0)
                canvasH = 0;

        clear_screen_and_drawing_state();

        if (globalData.isSelectingBuffer)
                lay_out_buffer_list(canvasW, canvasH);
        else
                lay_out_TextEdit(canvasW, canvasH, edit);

        //lay_out_rect(&windowDrawList, 0, 0, windowWidthInPixels, windowHeightInPixels, 128, 0, 0, 255);

        commit_all_dirty_textures(); //XXX
        draw_list(&numbersDrawList, linesX, linesY, linesW, linesH);
        draw_list(&contentsDrawList, textAreaX, textAreaY, textAreaW, textAreaH);
        draw_list(&statuslineDrawList, statuslineX, statuslineY, statuslineW, statuslineH);
        draw_list(&windowDrawList, 0, 0, windowWidthInPixels, windowHeightInPixels);
}
