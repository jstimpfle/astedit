#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/textedit.h>
#include <astedit/texteditloadsave.h>
#include <string.h>  // strlen()


/*
READ
*/

static void prepare_loading_from_file_to_textedit(void *param, FILEPOS filesizeInBytes)
{
        struct TextEdit *edit = param;
        /* TODO: must protect this section */
        edit->loading.isCompleted = 0;
        edit->loading.completedBytes = 0;
        edit->loading.totalBytes = filesizeInBytes;
        edit->loading.bufferFill = 0;
        edit->loading.timer = create_timer();
        start_timer(edit->loading.timer);
}

static void finalize_loading_from_file_to_textedit(void *param)
{
        struct TextEdit *edit = param;

        if (edit->loading.bufferFill > 0) {
                /* unfinished how to handle this? */
                log_postf("Warning: Input contains incomplete UTF-8 sequence at the end.");
        }

        /* TODO: must protect this section */
        edit->loading.isCompleted = 1;
        stop_timer(edit->loading.timer);
        report_timer(edit->loading.timer, "File load time");
        destroy_timer(edit->loading.timer);
}

static int flush_loadingBuffer_from_filereadthread(void *param)
{
        struct TextEdit *edit = param;
        ENSURE(edit->loading.isActive);
        ENSURE(edit->loading.isCompleted == 0);

        uint32_t utf8buf[LENGTH(edit->loading.buffer)];
        int utf8Fill;
        decode_utf8_span_and_move_rest_to_front(
                edit->loading.buffer,
                edit->loading.bufferFill,
                utf8buf,
                &edit->loading.bufferFill,
                &utf8Fill);
        insert_codepoints_into_textedit(edit, textrope_length(edit->rope), utf8buf, utf8Fill);

        edit->loading.completedBytes += utf8Fill;

        return 0;  /* report success */
}
void load_file_into_textedit(const char *filepath, int filepathLength, struct TextEdit *edit)
{
        struct FilereadThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_string_and_zeroterminate(ctx->filepath, filepath, filepathLength);
        ctx->param = edit;
        ctx->buffer = edit->loading.buffer;
        ctx->bufferSize = (int) sizeof edit->loading.buffer,
        ctx->bufferFill = &edit->loading.bufferFill;
        ctx->prepareFunc = &prepare_loading_from_file_to_textedit;
        ctx->finalizeFunc = &finalize_loading_from_file_to_textedit;
        ctx->flushBufferFunc = &flush_loadingBuffer_from_filereadthread;
        ctx->returnStatus = 1337; //XXX: "never changed from thread"

        struct OsThreadHandle *handle = create_and_start_thread(&read_file_thread_adapter, ctx);

        edit->loading.isActive = 1;
        edit->loading.threadCtx = ctx;
        edit->loading.threadHandle = handle;
}

/*
FILE WRITE
*/

static void prepare_writing_from_textedit_to_file(void *param)
{
        struct TextEdit *edit = param;
        FILEPOS totalBytes = textrope_length(edit->rope);
        edit->saving.isCompleted = 0;
        edit->saving.completedBytes = 0;
        edit->saving.totalBytes = totalBytes;
        edit->saving.bufferFill = 0;
        edit->saving.timer = create_timer();
        start_timer(edit->saving.timer);
}

static void finalize_writing_from_textedit_to_file(void *param)
{
        struct TextEdit *edit = param;
        edit->saving.isCompleted = 1;
        stop_timer(edit->saving.timer);
        report_timer(edit->saving.timer, "File load time");
        destroy_timer(edit->saving.timer);
}

static void fill_buffer_for_writing_to_file(void *param)
{
        struct TextEdit *edit = param;
        FILEPOS readStart = edit->saving.completedBytes;
        FILEPOS numToRead = sizeof edit->saving.buffer - edit->saving.bufferFill;
        if (numToRead > textrope_length(edit->rope) - readStart)
                numToRead = textrope_length(edit->rope) - readStart;
        FILEPOS numRead = copy_text_from_textrope(edit->rope, readStart,
                edit->saving.buffer + edit->saving.bufferFill, numToRead);
        edit->saving.bufferFill += cast_filepos_to_int(numRead);

        // we're not doing the decode/encode dance here, for now

        edit->saving.completedBytes += numRead;
}

void write_contents_from_textedit_to_file(struct TextEdit *edit, const char *filepath, int filepathLength)
{
        struct FilewriteThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_memory(ctx->filepath, filepath, filepathLength + 1);
        ctx->param = edit;
        ctx->buffer = &edit->saving.buffer[0]; // TODO
        ctx->bufferFill = &edit->saving.bufferFill;  // TODO
        ctx->bufferSize = sizeof edit->saving.buffer;
        ctx->prepareFunc = &prepare_writing_from_textedit_to_file;
        ctx->finalizeFunc = &finalize_writing_from_textedit_to_file;
        ctx->fillBufferFunc = &fill_buffer_for_writing_to_file;
        ctx->returnStatus = 0;

        struct OsThreadHandle *handle = create_and_start_thread(write_file_thread_adapter, ctx);

        edit->saving.isActive = 1;
        edit->saving.threadCtx = ctx;
        edit->saving.threadHandle = handle;
}
