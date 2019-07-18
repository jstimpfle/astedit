struct Textrope;

struct Textrope *create_textrope(void);
void destroy_textrope(struct Textrope *textrope);

int textrope_length(struct Textrope *rope);
void insert_text_into_textrope(struct Textrope *textrope, int offset, const char *text, int length);
void erase_text_from_textrope(struct Textrope *textrope, int offset, int length);
int copy_text_from_textrope(struct Textrope *rope, int offset, char *dstBuffer, int length);
void debug_print_textrope(struct Textrope *rope);
