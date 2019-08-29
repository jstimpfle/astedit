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
        edit->isLoadingCompleted = 0;
        edit->loadingCompletedBytes = 0;
        edit->loadingTotalBytes = filesizeInBytes;
        edit->loadingBufferFill = 0;
        edit->loadingTimer = create_timer();
        start_timer(edit->loadingTimer);
}

static void finalize_loading_from_file_to_textedit(void *param)
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
void load_file_into_textedit(const char *filepath, int filepathLength, struct TextEdit *edit)
{
        struct FilereadThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_string_and_zeroterminate(ctx->filepath, filepath, filepathLength);
        ctx->param = edit;
        ctx->buffer = edit->loadingBuffer;
        ctx->bufferSize = (int) sizeof edit->loadingBuffer,
        ctx->bufferFill = &edit->loadingBufferFill;
        ctx->prepareFunc = &prepare_loading_from_file_to_textedit;
        ctx->finalizeFunc = &finalize_loading_from_file_to_textedit;
        ctx->flushBufferFunc = &flush_loadingBuffer_from_filereadthread;
        ctx->returnStatus = 1337; //XXX: "never changed from thread"

        struct OsThreadHandle *handle = create_and_start_thread(&read_file_thread_adapter, ctx);

        edit->isLoading = 1;
        edit->loadingThreadCtx = ctx;
        edit->loadingThreadHandle = handle;
}

/*
FILE WRITE
*/

static void prepare_writing_from_textedit_to_file(void *param)
{
        struct TextEdit *edit = param;
        FILEPOS totalBytes = textrope_length(edit->rope);
        edit->isSavingCompleted = 0;
        edit->savingCompletedBytes = 0;
        edit->savingTotalBytes = totalBytes;
        edit->savingBufferFill = 0;
        edit->savingTimer = create_timer();
        start_timer(edit->savingTimer);
}

static void finalize_writing_from_textedit_to_file(void *param)
{
        struct TextEdit *edit = param;
        edit->isSavingCompleted = 1;
        stop_timer(edit->savingTimer);
        report_timer(edit->savingTimer, "File load time");
        destroy_timer(edit->savingTimer);
}

static void fill_buffer_for_writing_to_file(void *param)
{
        struct TextEdit *edit = param;
        FILEPOS readStart = edit->savingCompletedBytes;
        FILEPOS numToRead = sizeof edit->savingBuffer - edit->savingBufferFill;
        if (numToRead > textrope_length(edit->rope) - readStart)
                numToRead = textrope_length(edit->rope) - readStart;
        FILEPOS numRead = copy_text_from_textrope(edit->rope, readStart,
                edit->savingBuffer + edit->savingBufferFill, numToRead);
        edit->savingBufferFill += cast_filepos_to_int(numRead);

        // we're not doing the decode/encode dance here, for now

        edit->savingCompletedBytes += numRead;
}

void write_contents_from_textedit_to_file(struct TextEdit *edit, const char *filepath, int filepathLength)
{
        struct FilewriteThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_memory(ctx->filepath, filepath, filepathLength + 1);
        ctx->param = edit;
        ctx->buffer = &edit->savingBuffer[0]; // TODO
        ctx->bufferFill = &edit->savingBufferFill;  // TODO
        ctx->bufferSize = sizeof edit->savingBuffer;
        ctx->prepareFunc = &prepare_writing_from_textedit_to_file;
        ctx->finalizeFunc = &finalize_writing_from_textedit_to_file;
        ctx->fillBufferFunc = &fill_buffer_for_writing_to_file;
        ctx->returnStatus = 0;

        struct OsThreadHandle *handle = create_and_start_thread(write_file_thread_adapter, ctx);

        edit->isSaving = 1;
        edit->savingThreadCtx = ctx;
        edit->savingThreadHandle = handle;
}
