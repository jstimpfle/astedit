#include <astedit/astedit.h>
#include <astedit/filesystem.h>
#include <astedit/logging.h>
#include <sys/stat.h>

int query_filesize(FILE *f, FILEPOS *outSize)
{
        struct _stati64 _buf;

        int result = _fstati64(_fileno(f), &_buf);
        if (result != 0)
                return -1;

        if (_buf.st_size > FILEPOS_MAX - 1)
                // that really shouldn't happen. So we don't handle it.
                fatal("File too large!\n");

        *outSize = (FILEPOS)_buf.st_size;
        ENSURE(*outSize == _buf.st_size); // dirty (and probably technically invalid) way to check cast
        return 0;
}