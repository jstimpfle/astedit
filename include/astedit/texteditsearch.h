#include <astedit/textedit.h>

/* XXX: TODO: how to associate a search properly with a textedit? We'll need
 * a search handle, or need to link from the textedit to the search */
void start_search(struct TextEdit *edit, const char *pattern, int length);
void continue_search(struct TextEdit *edit);
void end_search(struct TextEdit *edit);
