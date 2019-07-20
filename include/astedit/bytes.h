#ifndef ASTEDIT_BYTES_H_INCLUDED
#define ASTEDIT_BYTES_H_INCLUDED

#define ZERO_MEMORY(ptr) zero_memory((ptr), sizeof *(ptr))
#define ZERO_ARRAY(ptr, numElems) zero_array((ptr), (numElems), sizeof *(ptr))
#define COPY_MEMORY(dst, src) copy_memory((dst), (src), sizeof *(dst))
#define COPY_ARRAY(dst, src, numElems) copy_array((dst), (src), (numElems), sizeof *(dst))

void zero_memory(void *ptr, int numBytes);
void zero_array(void *ptr, int numElems, int elemSize);
void copy_memory(void *dst, const void *src, int numBytes);
void copy_array(void *dst, const void *src, int numElems, int elemSize);
void move_memory(void *ptr, int distance, int numBytes);

#endif