#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <astedit/utf8.h>
#include <astedit/textedit.h>
#include <astedit/texteditloadsave.h>


int check_if_loading_completed_and_if_so_then_cleanup(struct TextEditLoadingCtx *ctx)
{
        if (!ctx->isActive)
                return 0;
        if (!check_if_thread_has_exited(ctx->threadHandle))
                return 0;
        dispose_thread(ctx->threadHandle);
        destroy_mutex(ctx->mutex);
        FREE_MEMORY(&ctx->filereadThreadCtx);
        /* TODO: check for load errors */
        ctx->isActive = 0;
        return 1;
}

int check_if_saving_completed_and_if_so_then_cleanup(struct TextEditSavingCtx *ctx)
{
        if (!ctx->isActive)
                return 0;
        if (!check_if_thread_has_exited(ctx->threadHandle))
                return 0;
        dispose_thread(ctx->threadHandle);
        destroy_mutex(ctx->mutex);
        FREE_MEMORY(&ctx->filewriteThreadCtx);
        /* TODO: check for save errors */
        ctx->isActive = 0;
        return 1;
}

void cancel_loading_file_to_textedit(struct TextEditLoadingCtx *ctx)
{
        ENSURE(ctx->isActive);
        ctx->filereadThreadCtx.isCancelled = 1;
        wait_for_thread_to_end(ctx->threadHandle);
        int r = check_if_loading_completed_and_if_so_then_cleanup(ctx); //XXX
        ENSURE(r != 0);
}

void cancel_saving_file_from_textedit(struct TextEditSavingCtx *ctx)
{
        ENSURE(ctx->isActive);
        ctx->filewriteThreadCtx.isCancelled = 1;
        wait_for_thread_to_end(ctx->threadHandle);
        int r = check_if_saving_completed_and_if_so_then_cleanup(ctx); //XXX
        ENSURE(r != 0);
}


/*
READ
*/

static void prepare_loading_from_file_to_textedit(void *param, FILEPOS filesizeInBytes)
{
        struct TextEditLoadingCtx *loading = param;
        /* TODO: must protect this section */
        loading->completedBytes = 0;
        loading->totalBytes = filesizeInBytes;
        loading->bufferFill = 0;
        start_timer(&loading->timer);
}

static void finalize_loading_from_file_to_textedit(void *param)
{
        struct TextEditLoadingCtx *loading = param;
        if (loading->bufferFill > 0) {
                /* unfinished how to handle this? */
                log_postf("Warning: Input contains incomplete UTF-8 sequence at the end.");
        }
        /* TODO: must protect this section */
        stop_timer(&loading->timer);
        report_timer(&loading->timer, "File load time");
}

static int flush_loadingBuffer_from_filereadthread(void *param)
{
        struct TextEditLoadingCtx *loading = param;
        ENSURE(loading->isActive);
#if 0
        uint32_t utf8buf[LENGTH(loading->buffer)];
        int utf8Fill;

        decode_utf8_span_and_move_rest_to_front(
                loading->buffer,
                loading->bufferFill,
                utf8buf,
                &loading->bufferFill,
                &utf8Fill);
        insert_codepoints_into_textedit(loading->edit, textrope_length(loading->edit->rope/*XXX*/), utf8buf, utf8Fill);
        loading->completedBytes += utf8Fill;
#else
        {

                FILEPOS currentPosition = textrope_length(loading->edit->rope/*XXX*/);
                FILEPOS nextPosition = currentPosition + loading->bufferFill;

                insert_text_into_textedit(loading->edit, currentPosition, loading->buffer, loading->bufferFill, nextPosition);
                loading->completedBytes += loading->bufferFill;
                loading->bufferFill = 0;
        }
#endif

        return 0;  /* report success */
}

void load_file_to_textedit(struct TextEditLoadingCtx *loading, const char *filepath, int filepathLength, struct TextEdit *edit)
{
        struct FilereadThreadCtx *ctx = &loading->filereadThreadCtx;
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_string_and_zeroterminate(ctx->filepath, filepath, filepathLength);
        ctx->isCancelled = 0;
        ctx->param = loading;
        ctx->buffer = loading->buffer;
        ctx->bufferSize = (int) sizeof loading->buffer;
        ctx->bufferFill = &loading->bufferFill;
        ctx->prepareFunc = &prepare_loading_from_file_to_textedit;
        ctx->finalizeFunc = &finalize_loading_from_file_to_textedit;
        ctx->flushBufferFunc = &flush_loadingBuffer_from_filereadthread;
        ctx->returnStatus = 1337; //XXX: "never changed from thread"

        loading->mutex = create_mutex();
        loading->edit = edit;
        loading->isActive = 1;
        loading->threadHandle = create_and_start_thread(&read_file_thread_adapter, ctx);
}

/*
FILE WRITE
*/

static void prepare_writing_from_textedit_to_file(void *param)
{
        struct TextEditSavingCtx *saving = param;
        FILEPOS totalBytes = textrope_length(saving->rope);
        saving->completedBytes = 0;
        saving->totalBytes = totalBytes;
        saving->bufferFill = 0;
        start_timer(&saving->timer);
}

static void finalize_writing_from_textedit_to_file(void *param)
{
        struct TextEditSavingCtx *saving = param;
        stop_timer(&saving->timer);
        report_timer(&saving->timer, "File save time");
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
        struct FilewriteThreadCtx *ctx = &saving->filewriteThreadCtx;
        ALLOC_MEMORY(&ctx->filepath, filepathLength + 1);
        copy_string_and_zeroterminate(ctx->filepath, filepath, filepathLength);
        ctx->isCancelled = 0;
        ctx->param = saving;
        ctx->buffer = &saving->buffer[0];
        ctx->bufferFill = &saving->bufferFill;
        ctx->bufferSize = sizeof saving->buffer;
        ctx->prepareFunc = &prepare_writing_from_textedit_to_file;
        ctx->finalizeFunc = &finalize_writing_from_textedit_to_file;
        ctx->fillBufferFunc = &fill_buffer_for_writing_to_file;
        ctx->returnStatus = 0;

        saving->mutex = create_mutex();
        saving->rope = rope;
        saving->isActive = 1;
        saving->threadHandle = create_and_start_thread(write_file_thread_adapter, ctx);
}
