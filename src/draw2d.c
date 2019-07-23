#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/window.h>
#include <astedit/font.h>
#include <astedit/gfx.h>
#include <astedit/textedit.h>
#include <astedit/utf8.h>
#include <astedit/draw2d.h>
#include <string.h> // memmove()

static struct ColorVertex2d colorVertexBuffer[3 * 1024];
//static struct TextureVertex2d fontVertexBuffer[3 * 1024];

static int colorVertexCount;
static int fontVertexCount;





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

void flush_font_texture_vertices(void)
{
        if (fontVertexCount > 0) {
                /*XXX TODO
                draw_alpha_texture_vertices(atlasTexture, fontVertexBuffer, fontVertexCount);
                */
                fontVertexCount = 0;
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
        draw_alpha_texture_vertices(verts, length);
}

void push_rgba_texture_vertices(struct TextureVertex2d *verts, int length)
{
        flush_color_vertices();
        flush_font_texture_vertices();
        draw_rgba_texture_vertices(verts, length);
}

void begin_frame(int x, int y, int w, int h)
{
        ENSURE(colorVertexCount == 0);
        ENSURE(fontVertexCount == 0);
        set_viewport_in_pixels(x, y, w, h);
}

void end_frame(void)
{
        flush_color_vertices();
        flush_font_texture_vertices();
}

static void fill_texture2d_rect(struct TextureVertex2d *rp,
        int x, int y, int w, int h,
        float texX, float texY, float texW, float texH)
{
        rp[0].x = x;     rp[0].y = y;       rp[0].z = 0;
        rp[1].x = x;     rp[1].y = y + h;   rp[1].z = 0;
        rp[2].x = x + w; rp[2].y = y + h;   rp[2].z = 0;
        rp[3].x = x;     rp[3].y = y;       rp[3].z = 0;
        rp[4].x = x + w; rp[4].y = y + h;   rp[4].z = 0;
        rp[5].x = x + w; rp[5].y = y;       rp[5].z = 0;

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
        rectpoints[0].x = x;     rectpoints[0].y = y;       rectpoints[0].z = 0;
        rectpoints[1].x = x;     rectpoints[1].y = y + h;   rectpoints[1].z = 0;
        rectpoints[2].x = x + w; rectpoints[2].y = y + h;   rectpoints[2].z = 0;
        rectpoints[3].x = x;     rectpoints[3].y = y;       rectpoints[3].z = 0;
        rectpoints[4].x = x + w; rectpoints[4].y = y + h;   rectpoints[4].z = 0;
        rectpoints[5].x = x + w; rectpoints[5].y = y;       rectpoints[5].z = 0;
        for (int i = 0; i < 6; i++) {
                rectpoints[i].r = r / 255.0f;
                rectpoints[i].g = g / 255.0f;
                rectpoints[i].b = b / 255.0f;
                rectpoints[i].a = a / 255.0f;
        }
        push_color_vertices(rectpoints, LENGTH(rectpoints));
}

void draw_rgba_texture_rect(int x, int y, int w, int h,
        float texX, float texY, float texW, float texH, Texture texture)
{
        static struct TextureVertex2d rp[6];

        fill_texture2d_rect(&rp[0], x, y, w, h, texX, texY, texW, texH);

        for (int i = 0; i < 6; i++)
                rp[i].tex = texture;

        push_rgba_texture_vertices(rp, LENGTH(rp));
}

void draw_alpha_texture_rect(int x, int y, int w, int h,
        float texX, float texY, float texW, float texH, Texture texture)
{
        static struct TextureVertex2d rp[6];

        fill_texture2d_rect(&rp[0], x, y, w, h, texX, texY, texW, texH);

        for (int i = 0; i < 6; i++)
                rp[i].tex = texture;

        push_alpha_texture_vertices(rp, LENGTH(rp));
}

struct DrawCursor {
        int xLeft;
        int fontSize;
        int ascender;
        int lineHeight;
        int x;
        int y;
        int codepointpos;
};

enum {
        DRAWSTRING_NORMAL,
        DRAWSTRING_HIGHLIGHT,
};

void draw_region(struct DrawCursor *cursor, uint32_t *text, int start, int end, int drawstringKind)
{
        int i = start;
        while (i < end) {
                int j = i;
                while (j < end && text[j] != '\n')
                        j++;

                int xEnd = draw_glyph_span(FONTFACE_REGULAR,
                        cursor->fontSize, text + i, j - i,
                        cursor->x, cursor->y);
                if (drawstringKind == DRAWSTRING_HIGHLIGHT)
                        draw_colored_rect(cursor->x, cursor->y - cursor->ascender,
                                xEnd - cursor->x, cursor->lineHeight,
                                0, 0, 192, 32);
                cursor->x = xEnd;

                i = j;
                if (i < end && text[i] == '\n') {
                        i++;
                        cursor->x = cursor->xLeft;
                        cursor->y += cursor->lineHeight;
                }
        }
}

void draw_text_file(const char *text, int length, int markStart, int markEnd)
{
        struct DrawCursor cursor;
        cursor.xLeft = 20;
        cursor.fontSize = 25;
        cursor.ascender = 20;
        cursor.lineHeight = 30;
        cursor.x = cursor.xLeft;
        cursor.y = 20;

        if (length == 0) return;  // for debugging
        
        if (markStart < 0) markStart = 0;
        if (markEnd < 0) markEnd = 0;
        if (markStart >= length) markStart = length;
        if (markEnd >= length) markEnd = length;

        ENSURE(markStart <= markEnd);

        int textpos = 0;
        int codepointpos = 0;
        uint32_t buffer[64];
        int bufferFill = 0;
        while (textpos < length) {
                int drawstringKind;
                int maxCodepoints;
                if (codepointpos < markStart) {
                        drawstringKind = DRAWSTRING_NORMAL;
                        maxCodepoints = minInt(markStart - codepointpos, LENGTH(buffer));
                }
                else if (codepointpos < markEnd) {
                        drawstringKind = DRAWSTRING_HIGHLIGHT;
                        maxCodepoints = minInt(LENGTH(buffer), markEnd - markStart);
                }
                else {
                        drawstringKind = DRAWSTRING_NORMAL;
                        maxCodepoints = LENGTH(buffer);
                }

                decode_utf8_span(text, textpos, length, &buffer[0], maxCodepoints, &textpos, &bufferFill);
                draw_region(&cursor, &buffer[0], 0, bufferFill, drawstringKind);
                codepointpos += bufferFill;
        }
}


static void draw_codepoints_with_cursor(struct DrawCursor *cursor, uint32_t *codepoints, int length, int markStart, int markEnd)
{
        int start = 0;
        while (start < length) {
                int numCodepointsRemain = length - start;
                int drawstringKind;
                int numCodepoints;
                if (cursor->codepointpos < markStart) {
                        drawstringKind = DRAWSTRING_NORMAL;
                        numCodepoints = minInt(numCodepointsRemain, markStart - cursor->codepointpos);
                }
                else if (cursor->codepointpos < markEnd) {
                        drawstringKind = DRAWSTRING_HIGHLIGHT;
                        numCodepoints = minInt(numCodepointsRemain, markEnd - cursor->codepointpos);
                }
                else {
                        drawstringKind = DRAWSTRING_NORMAL;
                        numCodepoints = numCodepointsRemain;
                }
                draw_region(cursor, codepoints, start, start + numCodepoints, drawstringKind);
                start += numCodepoints;
                cursor->codepointpos += numCodepoints;
        }
}

void draw_TextEdit(struct TextEdit *edit, int markStart, int markEnd)
{
        struct DrawCursor cursor;
        cursor.xLeft = 20;
        cursor.fontSize = 25;
        cursor.ascender = 20;
        cursor.lineHeight = 30;
        cursor.x = cursor.xLeft;
        cursor.y = 20;
        cursor.codepointpos = 0;

        int length = textedit_length_in_bytes(edit);

        if (length == 0) return;  // for debugging

        if (markStart < 0) markStart = 0;
        if (markEnd < 0) markEnd = 0;
        if (markStart >= length) markStart = length;
        if (markEnd >= length) markEnd = length;

        ENSURE(markStart <= markEnd);

        char readbuffer[256];
        uint32_t codepointBuffer[LENGTH(readbuffer)];

        int readbufferFill = 0;
        int readPositionInBytes = 0;  // textedit

        for (;;) {
                // for development / debugging (we still need to make the
                // drawing more effective and also introduce bounding boxes for
                // font culling)
                if (readPositionInBytes > 512)
                        break;

                {
                        int remainingSpace = LENGTH(readbuffer) - readbufferFill;
                        int texteditLengthInBytes = textedit_length_in_bytes(edit);
                        // (This is only true if the textedit doesn't change during the lifetime of this read buffer)
                        ENSURE(readPositionInBytes <= texteditLengthInBytes);
                        int editBytesAvailable = texteditLengthInBytes - readPositionInBytes;

                        int maxReadLength = remainingSpace;
                        if (maxReadLength > editBytesAvailable)
                                remainingSpace = editBytesAvailable;

                        int numBytesRead = read_from_textedit(edit, readPositionInBytes,
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

                draw_codepoints_with_cursor(&cursor, codepointBuffer, numCodepointsDecoded, markStart, markEnd);
        }
}

void testdraw(struct TextEdit *edit)
{
        begin_frame(100, 100, windowWidthInPixels - 100, windowHeightInPixels - 100);
        clear_screen_and_drawing_state();

        set_2d_coordinate_system(0.0f, 0.0f, (float)(windowWidthInPixels - 100), (float)(windowHeightInPixels - 100));

        //draw_colored_rect(200, 200, 200, 200, 128, 128, 128, 224);
        //draw_text_file(edit->contents, edit->length, 3, 5);

        draw_TextEdit(edit, edit->cursorCodepointPosition, edit->cursorCodepointPosition + 1);
        

        end_frame();
        swap_buffers();
}
