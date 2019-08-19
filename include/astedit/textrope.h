#ifndef TEXTROPE_H_INCLUDED
#define TEXTROPE_H_INCLUDED

#ifndef ASTEDIT_ASTEDIT_H_INCLUDED
#include <astedit/astedit.h>
#endif

#ifndef ASTEDIT_FILEPOSITIONS_H_INCLUDED
#include <astedit/filepositions.h>
#endif

struct Textrope;

struct Textrope *create_textrope(void);
void destroy_textrope(struct Textrope *textrope);

FILEPOS textrope_length(struct Textrope *rope);
FILEPOS textrope_number_of_lines(struct Textrope *rope);
FILEPOS textrope_number_of_codepoints(struct Textrope *rope);

/*
this will return one more than the actual number of '\n' characters
in the text if the text length is > 0 and the last character isn't '\n'
*/
FILEPOS textrope_number_of_lines_quirky(struct Textrope *rope);

void compute_line_number_and_codepoint_position(struct Textrope *rope, FILEPOS pos, FILEPOS *outLinenumber, FILEPOS *outCodepointPosition);
FILEPOS compute_line_number(struct Textrope *rope, FILEPOS pos);
FILEPOS compute_codepoint_position(struct Textrope *rope, FILEPOS pos);

FILEPOS compute_pos_of_line(struct Textrope *rope, FILEPOS lineNumber);
FILEPOS compute_pos_of_codepoint(struct Textrope *rope, FILEPOS codepointPos);

void insert_text_into_textrope(struct Textrope *textrope, FILEPOS offset, const char *text, FILEPOS length);
void erase_text_from_textrope(struct Textrope *textrope, FILEPOS offset, FILEPOS length);
FILEPOS copy_text_from_textrope(struct Textrope *rope, FILEPOS offset, char *dstBuffer, FILEPOS length);


void debug_check_textrope(struct Textrope *rope);
void debug_print_textrope(struct Textrope *rope);
void print_textrope_statistics(struct Textrope *rope);

#endif