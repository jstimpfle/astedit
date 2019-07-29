struct Textrope;

struct Textrope *create_textrope(void);
void destroy_textrope(struct Textrope *textrope);

int textrope_length(struct Textrope *rope);
int textrope_number_of_lines(struct Textrope *rope);
int textrope_number_of_codepoints(struct Textrope *rope);

/*
this will return one more than the actual number of '\n' characters
in the text if the text length is > 0 and the last character isn't '\n'
*/
int textrope_number_of_lines_quirky(struct Textrope *rope);


void compute_line_number_and_codepoint_position(struct Textrope *rope, int pos, int *outLinenumber, int *outCodepointPosition);
int compute_line_number(struct Textrope *rope, int pos);
int compute_codepoint_position(struct Textrope *rope, int pos);

int compute_pos_of_line(struct Textrope *rope, int lineNumber);
int compute_pos_of_codepoint(struct Textrope *rope, int codepointPos);

void insert_text_into_textrope(struct Textrope *textrope, int offset, const char *text, int length);
void erase_text_from_textrope(struct Textrope *textrope, int offset, int length);
int copy_text_from_textrope(struct Textrope *rope, int offset, char *dstBuffer, int length);


void debug_check_textrope(struct Textrope *rope);
void debug_print_textrope(struct Textrope *rope);
void print_textrope_statistics(struct Textrope *rope);