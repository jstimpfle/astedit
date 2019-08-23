#ifndef ASTEDIT_FILESYSTEM_H_INCLUDED
#define ASTEDIT_FILESYSTEM_H_INCLUDED

#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#include <astedit/astedit.h>
#endif

#ifndef ASTEDIT_FILEPOSITIONS_H_INCLUDED
#include <astedit/filepositions.h>  // FILEPOS
#endif

#include <stdio.h> //XXX: libc's FILE

int query_filesize(FILE *f, FILEPOS *outSize);  // returns 0 and fills outSize on success.

#endif
