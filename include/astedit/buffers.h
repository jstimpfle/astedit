#ifndef ASTEDIT_BUFFERS_H_INCLUDED
#define ASTEDIT_BUFFERS_H_INCLUDED

#include <astedit/astedit.h>

struct Buffer;

void switch_to_buffer(struct Buffer *buffer);
struct Buffer *create_new_buffer(const char *name);
void destroy_buffer(struct Buffer *buffer);

#endif
