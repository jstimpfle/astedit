#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/window.h>  // XXX shouldWindowClose
#include <astedit/filesystem.h>
#include <astedit/filereadthread.h>
#include <stdio.h>


void read_file_thread(struct FilereadThreadCtx *ctx)
{
        int returnStatus = 0;

        FILE *f = fopen(ctx->filepath, "rb");
        if (!f) {
                log_postf("Failed to open file %s", ctx->filepath);
                returnStatus = -1;
                goto out1;
        }

        FILEPOS filesize;
        if (query_filesize(f, &filesize) != 0) {
                log_postf("Failed to query file size of %s", ctx->filepath);
                returnStatus = -1;
                goto out2;
        }

        ctx->prepareFunc(ctx->param, filesize);

        for (;;) {
                if (shouldWindowClose) {
                        /* Thread should terminate. We will abort the loading.
                        We might want a more general inter-thread communication system.
                        For now, this variable is good enough (although technically we
                        need to protect the access to make sure that we get the new value
                        if it was updated from another thread). */
                        returnStatus = -1;  // TODO: choose a different return code? This is not strictly an error.
                        goto out3;
                }

                size_t n = fread(ctx->buffer + *ctx->bufferFill, 1,
                                        ctx->bufferSize - *ctx->bufferFill, f);

                if (n == 0)
                        /* EOF. Ignore remaining undecoded bytes */
                        break;

                *ctx->bufferFill += (int) n;

                int r = (*ctx->flushBufferFunc)(ctx->param);

                if (r == -1) {
                        returnStatus = -1;
                        goto out3;
                }
        }

        if (ferror(f)) {
                log_postf("Errors while reading from file %s", ctx->filepath);
                returnStatus = -1;
        }

out3:
        ctx->finalizeFunc(ctx->param);
out2:
        fclose(f);
out1:
        ctx->returnStatus = returnStatus;
}

void read_file_thread_adapter(void *ctx)
{
        read_file_thread(ctx);
}
