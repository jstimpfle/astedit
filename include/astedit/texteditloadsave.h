#ifndef ASTEDIT_TEXTEDITLOADSAVE_H_INCLUDED
#define ASTEDIT_TEXTEDITLOADSAVE_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/textedit.h>

void load_file_into_textedit(const char *filepath, int filepathLength, struct TextEdit *edit);
void write_contents_from_textedit_to_file(struct TextEdit *edit, const char *filepath, int filepathLength);

#endif
