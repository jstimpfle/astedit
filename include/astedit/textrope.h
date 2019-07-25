struct Textrope;

struct Textrope *create_textrope(void);
void destroy_textrope(struct Textrope *textrope);

int textrope_length(struct Textrope *rope);

int compute_linenumber_from_offset_zerobased(struct Textrope *rope, int pos);
int compute_offset_from_linenumber_zerobased(struct Textrope *rope, int line);

void insert_text_into_textrope(struct Textrope *textrope, int offset, const char *text, int length);
void erase_text_from_textrope(struct Textrope *textrope, int offset, int length);
int copy_text_from_textrope(struct Textrope *rope, int offset, char *dstBuffer, int length);


void debug_print_textrope(struct Textrope *rope);
void print_textrope_statistics(struct Textrope *rope);

