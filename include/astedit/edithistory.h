#ifndef ASTEDIT_EDITHISTORY_H_INCLUDED
#define ASTEDIT_EDITHISTORY_H_INCLUDED

#include <astedit/astedit.h>
#include <astedit/textedit.h>

void record_insert_operation(struct TextEdit *edit, FILEPOS insertionPoint, FILEPOS length,
                             FILEPOS previousCursorPosition, FILEPOS nextCursorPosition);
void record_delete_operation(struct TextEdit *edit, FILEPOS deletionPoint, FILEPOS length,
                             FILEPOS previousCursorPosition, FILEPOS nextCursorPosition);
int undo_last_edit_operation(struct TextEdit *edit);
int redo_next_edit_operation(struct TextEdit *edit);

#endif
