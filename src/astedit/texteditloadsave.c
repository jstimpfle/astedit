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
        struct TextEditLoadingCtx *loading = param;
        /* TODO: must protect this section */
        loading->isCompleted = 0;
        loading->completedBytes = 0;
        loading->totalBytes = filesizeInBytes;
        loading->bufferFill = 0;
        loading->timer = create_timer();
        start_timer(loading->timer);
}

static void finalize_loading_from_file_to_textedit(void *param)
{
        struct TextEditLoadingCtx *loading = param;
        if (loading->bufferFill > 0) {
                /* unfinished how to handle this? */
                log_postf("Warning: Input contains incomplete UTF-8 sequence at the end.");
        }
        /* TODO: must protect this section */
        loading->isCompleted = 1;
        stop_timer(loading->timer);
        report_timer(loading->timer, "File load time");
        destroy_timer(loading->timer);
}

static int flush_loadingBuffer_from_filereadthread(void *param)
{
        struct TextEditLoadingCtx *loading = param;
        ENSURE(loading->isActive);
        ENSURE(loading->isCompleted == 0);
        uint32_t utf8buf[LENGTH(loading->buffer)];
        int utf8Fill;
        decode_utf8_span_and_move_rest_to_front(
                loading->buffer,
                loading->bufferFill,
                utf8buf,
                &loading->bufferFill,
                &utf8Fill);
        insert_codepoints_into_textrope(loading->rope, textrope_length(loading->rope), utf8buf, utf8Fill);

        loading->completedBytes += utf8Fill;

        return 0;  /* report success */
}
void load_file_to_textrope(struct TextEditLoadingCtx *loading, const char *filepath, int filepathLength, struct Textrope *rope)
{
        struct FilereadThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1); // TODO: check it's freed?
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_string_and_zeroterminate(ctx->filepath, filepath, filepathLength);
        ctx->param = loading;
        ctx->buffer = loading->buffer;
        ctx->bufferSize = (int) sizeof loading->buffer;
        ctx->bufferFill = &loading->bufferFill;
        ctx->prepareFunc = &prepare_loading_from_file_to_textedit;
        ctx->finalizeFunc = &finalize_loading_from_file_to_textedit;
        ctx->flushBufferFunc = &flush_loadingBuffer_from_filereadthread;
        ctx->returnStatus = 1337; //XXX: "never changed from thread"

        loading->rope = rope;
        loading->isActive = 1;
        loading->threadCtx = ctx;
        loading->threadHandle = create_and_start_thread(&read_file_thread_adapter, ctx);
}

/*
FILE WRITE
*/

static void prepare_writing_from_textedit_to_file(void *param)
{
        struct TextEditSavingCtx *saving = param;
        FILEPOS totalBytes = textrope_length(saving->rope);
        saving->isCompleted = 0;
        saving->completedBytes = 0;
        saving->totalBytes = totalBytes;
        saving->bufferFill = 0;
        saving->timer = create_timer();
        start_timer(saving->timer);
}

static void finalize_writing_from_textedit_to_file(void *param)
{
        struct TextEditSavingCtx *saving = param;
        saving->isCompleted = 1;
        stop_timer(saving->timer);
        report_timer(saving->timer, "File load time");
        destroy_timer(saving->timer);
}

static void fill_buffer_for_writing_to_file(void *param)
{
        struct TextEditSavingCtx *saving = param;
        FILEPOS readStart = saving->completedBytes;
        FILEPOS numToRead = sizeof saving->buffer - saving->bufferFill;
        if (numToRead > textrope_length(saving->rope) - readStart)
                numToRead = textrope_length(saving->rope) - readStart;
        FILEPOS numRead = copy_text_from_textrope(saving->rope, readStart,
                saving->buffer + saving->bufferFill, numToRead);
        saving->bufferFill += cast_filepos_to_int(numRead);

        // we're not doing the decode/encode dance here, for now
        saving->completedBytes += numRead;
}

void write_textrope_contents_to_file(struct TextEditSavingCtx *saving, struct Textrope *rope, const char *filepath, int filepathLength)
{
        struct FilewriteThreadCtx *ctx;
        ALLOC_MEMORY(&ctx, 1);  // XXX check if freed properly
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_memory(ctx->filepath, filepath, filepathLength + 1);
        ctx->param = saving;
        ctx->buffer = &saving->buffer[0];
        ctx->bufferFill = &saving->bufferFill;
        ctx->bufferSize = sizeof saving->buffer;
        ctx->prepareFunc = &prepare_writing_from_textedit_to_file;
        ctx->finalizeFunc = &finalize_writing_from_textedit_to_file;
        ctx->fillBufferFunc = &fill_buffer_for_writing_to_file;
        ctx->returnStatus = 0;

        saving->rope = rope;
        saving->isActive = 1;
        saving->threadCtx = ctx;
        saving->threadHandle = create_and_start_thread(write_file_thread_adapter, ctx);
}
