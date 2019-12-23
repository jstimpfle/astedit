#include <astedit/textedit.h>

/* XXX: TODO: how to associate a search properly with a textedit? We'll need
 * a search handle, or need to link from the textedit to the search */
void setup_search(struct TextEdit *edit, const char *pattern, int length);
int search_next_match(struct TextEdit *edit, FILEPOS *matchStart, FILEPOS *matchEnd);
void teardown_search(struct TextEdit *edit);
