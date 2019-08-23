#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/logging.h>
#include <astedit/filereadthread.h>
#include <astedit/textrope.h>
#include <astedit/textedit.h>

/* test */
#include <blunt/lex.h>


void get_selected_range_in_bytes(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd)
{
        ENSURE(edit->isSelectionMode);
        if (edit->cursorBytePosition < edit->selectionStartBytePosition) {
                *outStart = edit->cursorBytePosition;
                *outOnePastEnd = edit->selectionStartBytePosition;
        }
        else {
                *outStart = edit->selectionStartBytePosition;
                *outOnePastEnd = edit->cursorBytePosition;
        };
}

void get_selected_range_in_codepoints(struct TextEdit *edit, FILEPOS *outStart, FILEPOS *outOnePastEnd)
{
        FILEPOS start;
        FILEPOS onePastEnd;
        get_selected_range_in_bytes(edit, &start, &onePastEnd);
        *outStart = compute_codepoint_position(edit->rope, start);
        *outOnePastEnd = compute_codepoint_position(edit->rope, onePastEnd);
}



void move_view_minimally_to_display_line(struct TextEdit *edit, FILEPOS lineNumber)
{
        if (edit->firstLineDisplayed > lineNumber)
                edit->firstLineDisplayed = lineNumber;
        if (edit->firstLineDisplayed < lineNumber - edit->numberOfLinesDisplayed + 1)
                edit->firstLineDisplayed = lineNumber - edit->numberOfLinesDisplayed + 1;
}

void move_view_minimally_to_display_cursor(struct TextEdit *edit)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        move_view_minimally_to_display_line(edit, lineNumber);
}

void move_cursor_to_byte_position(struct TextEdit *edit, FILEPOS pos, int isSelecting)
{
        if (!edit->isSelectionMode && isSelecting)
                edit->selectionStartBytePosition = edit->cursorBytePosition;
        edit->isSelectionMode = isSelecting;

        edit->cursorBytePosition = pos;
        move_view_minimally_to_display_cursor(edit);
        //log_postf("Cursor is in line %d", compute_line_number(edit->rope, pos));
}

void move_cursor_to_codepoint(struct TextEdit *edit, FILEPOS codepointPos, int isSelecting)
{
        FILEPOS pos = compute_pos_of_codepoint(edit->rope, codepointPos);
        move_cursor_to_byte_position(edit, pos, isSelecting);
}

void move_cursor_left(struct TextEdit *edit, int isSelecting)
{
        FILEPOS codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        //int totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition > 0)
                move_cursor_to_codepoint(edit, codepointPosition - 1, isSelecting);
}

void move_cursor_right(struct TextEdit *edit, int isSelecting)
{
        FILEPOS codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        FILEPOS totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition < totalCodepoints)  // we may move one past end
                move_cursor_to_codepoint(edit, codepointPosition + 1, isSelecting);
}

void move_cursor_to_line_and_column(struct TextEdit *edit, FILEPOS lineNumber, FILEPOS codepointColumn, int isSelecting)
{
        FILEPOS linePos = compute_pos_of_line(edit->rope, lineNumber);
        FILEPOS lineCodepointPosition = compute_codepoint_position(edit->rope, linePos);
        FILEPOS nextLinePos = compute_pos_of_line(edit->rope, lineNumber + 1);
        FILEPOS nextLineCodepointPosition = compute_codepoint_position(edit->rope, nextLinePos);

        FILEPOS codepointsInLine = nextLineCodepointPosition - lineCodepointPosition;
        if (codepointColumn > codepointsInLine) {
                codepointColumn = codepointsInLine;
                if (codepointColumn > 0)  /* don't place on the last column (newline) but on the column before that. Good idea? */
                        codepointColumn--;
        }

        move_cursor_to_codepoint(edit, lineCodepointPosition + codepointColumn, isSelecting);
}

void move_cursor_lines_relative(struct TextEdit *edit, FILEPOS linesDiff, int isSelecting)
{
        FILEPOS oldLineNumber;
        FILEPOS oldCodepointPosition;
        compute_line_number_and_codepoint_position(edit->rope, edit->cursorBytePosition,
                &oldLineNumber, &oldCodepointPosition);

        FILEPOS newLineNumber = oldLineNumber + linesDiff;

        if (newLineNumber < 0)
                newLineNumber = 0;
        else if (newLineNumber >= textrope_number_of_lines(edit->rope))
                newLineNumber = textrope_number_of_lines(edit->rope);

        FILEPOS oldLinePos = compute_pos_of_line(edit->rope, oldLineNumber);
        FILEPOS oldLineCodepointPosition = compute_codepoint_position(edit->rope, oldLinePos);
        FILEPOS codepointColumn = oldCodepointPosition - oldLineCodepointPosition;
        move_cursor_to_line_and_column(edit, newLineNumber, codepointColumn, isSelecting);
}

void move_cursor_up(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, -1, isSelecting);
}

void move_cursor_down(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, +1, isSelecting);
}

void move_cursor_to_beginning_of_line(struct TextEdit *edit, int isSelecting)
{
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        FILEPOS pos = compute_pos_of_line(edit->rope, lineNumber);
        move_cursor_to_byte_position(edit, pos, isSelecting);
}

void move_cursor_to_end_of_line(struct TextEdit *edit, int isSelecting)
{
        FILEPOS numberOfLines = textrope_number_of_lines_quirky(edit->rope);
        FILEPOS lineNumber = compute_line_number(edit->rope, edit->cursorBytePosition);
        if (lineNumber >= numberOfLines)
                return;  // i think this happens when the cursor is editing at the ending position of the text
        FILEPOS pos = compute_pos_of_line(edit->rope, lineNumber + 1);
        FILEPOS codepointPos = compute_codepoint_position(edit->rope, pos);
        ENSURE(codepointPos > 0);
        move_cursor_to_codepoint(edit, codepointPos - 1, isSelecting);
}

void move_to_first_line(struct TextEdit *edit, int isSelecting)
{
        move_cursor_to_line_and_column(edit, 0, 0, isSelecting);
}

void move_to_last_line(struct TextEdit *edit, int isSelecting)
{
        move_cursor_to_line_and_column(edit,
                                textrope_number_of_lines(edit->rope),
                                0, isSelecting);
}

enum { LINES_PER_PAGE = 15 };

void scroll_up_one_page(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, -LINES_PER_PAGE, isSelecting);
}

void scroll_down_one_page(struct TextEdit *edit, int isSelecting)
{
        move_cursor_lines_relative(edit, LINES_PER_PAGE, isSelecting);
}

void insert_codepoints_into_textedit(struct TextEdit *edit, FILEPOS insertPos, uint32_t *codepoints, int numCodepoints)
{
        UNUSED(edit);
        int codepointsPos = 0;
        FILEPOS ropePos = insertPos;
        while (codepointsPos < numCodepoints) {
                char buf[512];
                int bufFill;
                encode_utf8_span(codepoints, codepointsPos, numCodepoints, buf, sizeof buf, &codepointsPos, &bufFill);
                insert_text_into_textrope(edit->rope, ropePos, &buf[0], bufFill);
                ropePos += bufFill;
        }
}

void insert_codepoint_into_textedit(struct TextEdit *edit, uint32_t codepoint)
{
        insert_codepoints_into_textedit(edit, edit->cursorBytePosition, &codepoint, 1);
        move_cursor_right(edit, 0);  // XXX
}

void erase_selected_in_TextEdit(struct TextEdit *edit)
{
        ENSURE(edit->isSelectionMode);
        FILEPOS start;
        FILEPOS onePastEnd;
        get_selected_range_in_bytes(edit, &start, &onePastEnd);
        erase_text_from_textrope(edit->rope, start, onePastEnd - start);
        edit->isSelectionMode = 0;
        edit->cursorBytePosition = start;
}

void erase_forwards_in_TextEdit(struct TextEdit *edit)
{
        FILEPOS codepointPosition = compute_codepoint_position(edit->rope, edit->cursorBytePosition);
        FILEPOS totalCodepoints = textrope_number_of_codepoints(edit->rope);
        if (codepointPosition < totalCodepoints) {
                FILEPOS start = edit->cursorBytePosition;
                FILEPOS end = compute_pos_of_codepoint(edit->rope, codepointPosition + 1);
                if (start < end)
                        erase_text_from_textrope(edit->rope, start, end - start);
        }
}

void erase_backwards_in_TextEdit(struct TextEdit *edit)
{
        FILEPOS end = edit->cursorBytePosition;
        move_cursor_left(edit, 0); //XXX
        FILEPOS start = edit->cursorBytePosition;
        if (start < end)
                erase_text_from_textrope(edit->rope, start, end - start);
}

void init_TextEdit(struct TextEdit *edit)
{
        UNUSED(edit);
        edit->rope = create_textrope();

        edit->cursorBytePosition = 0;
        edit->firstLineDisplayed = 0;
        edit->numberOfLinesDisplayed = 15;  // XXX need some mechanism to set and update this

        edit->isSelectionMode = 0;
        edit->selectionStartBytePosition = 0;

        edit->isLoading = 0;
        edit->loadingCompletedBytes = 0;
        edit->loadingTotalBytes = 0;
        edit->loadingThreadHandle = NULL;
}

void exit_TextEdit(struct TextEdit *edit)
{
        if (edit->loadingThreadHandle != NULL) {
                dispose_file_read_thread(edit->loadingThreadHandle);
                edit->loadingThreadHandle = NULL;
        }

        destroy_textrope(edit->rope);
}


static void prepare_reading_from_filereadthread(void *param, FILEPOS filesizeInBytes)
{
        struct TextEdit *edit = param;
        /* TODO: must protect this section */
        edit->isLoadingCompleted = 0;
        edit->loadingCompletedBytes = 0;
        edit->loadingTotalBytes = filesizeInBytes;
        edit->loadingBufferFill = 0;
        edit->loadingTimer = create_timer();
        start_timer(edit->loadingTimer);
}

static void finalize_reading_from_filereadthread(void *param)
{
        struct TextEdit *edit = param;

        if (edit->loadingBufferFill > 0) {
                /* unfinished how to handle this? */
                log_postf("Warning: Input contains incomplete UTF-8 sequence at the end.");
        }

        /* TODO: must protect this section */
        edit->isLoadingCompleted = 1;
        stop_timer(edit->loadingTimer);
        report_timer(edit->loadingTimer, "File load time");
        destroy_timer(edit->loadingTimer);
}

static int flush_loadingBuffer_from_filereadthread(void *param)
{
        struct TextEdit *edit = param;
        ENSURE(edit->isLoading);
        ENSURE(edit->isLoadingCompleted == 0);

        uint32_t utf8buf[LENGTH(edit->loadingBuffer)];
        int utf8Fill;
        decode_utf8_span_and_move_rest_to_front(
                edit->loadingBuffer,
                edit->loadingBufferFill,
                utf8buf,
                &edit->loadingBufferFill,
                &utf8Fill);
        insert_codepoints_into_textedit(edit, textrope_length(edit->rope), utf8buf, utf8Fill);

        edit->loadingCompletedBytes += utf8Fill;

        return 0;  /* report success */
}

/* TODO: maybe introduce TIMETICK event or sth like that? */
void update_textedit(struct TextEdit *edit)
{
        if (edit->isLoading && edit->isLoadingCompleted) {
                /* TODO: check for load errors */
                edit->isLoading = 0;
                dispose_file_read_thread(edit->loadingThreadHandle);
                FREE_MEMORY(&edit->loadingThreadCtx->filepath);
                FREE_MEMORY(&edit->loadingThreadCtx);
                edit->isLoadingCompleted = 0;
                edit->loadingThreadCtx = NULL;  // XXX should FREE_MEMORY do that already?
                edit->loadingThreadHandle = NULL;  // XXX should FREE_MEMORY do that already?
        }
}

void textedit_test_init(struct TextEdit *edit, const char *filepath)
{
        int filepathLen = (int)strlen(filepath);

        struct FilereadThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);
        ALLOC_MEMORY(&ctx->filepath, filepathLen + 1);
        COPY_ARRAY(ctx->filepath, filepath, filepathLen + 1);
        ctx->param = edit;
        ctx->buffer = edit->loadingBuffer;
        ctx->bufferSize = (int) sizeof edit->loadingBuffer,
        ctx->bufferFill = &edit->loadingBufferFill;
        ctx->prepareFunc = &prepare_reading_from_filereadthread;
        ctx->finalizeFunc = &finalize_reading_from_filereadthread;
        ctx->flushBufferFunc = &flush_loadingBuffer_from_filereadthread;
        ctx->returnStatus = 1337; //XXX: "never changed from thread"

        struct FilereadThreadHandle *handle = run_file_read_thread(ctx);

        edit->isLoading = 1;
        edit->loadingThreadCtx = ctx;
        edit->loadingThreadHandle = handle;
}
