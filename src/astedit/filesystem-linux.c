#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/filesystem.h>
#include <stdio.h>
#include <sys/stat.h>

int query_filesize(FILE *f, FILEPOS *outSize)
{
        struct stat buf;
        int r = fstat(fileno(f), &buf);
        if (r != 0) {
                log_postf("Error determining file size");
                return -1;
        }
        if (FILEPOS_MAX - 1 < buf.st_size)
                // really shouldn't happen
                fatalf("File too large!");
        *outSize = (FILEPOS) buf.st_size;
        ENSURE(*outSize == buf.st_size);
        return 0;
}
