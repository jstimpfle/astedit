#ifndef ASTEDIT_TEXTPOSITIONS_H_INCLUDED
#define ASTEDIT_TEXTPOSITIONS_H_INCLUDED

struct TextEdit;

struct FileCursor {
        FILEPOS bytePosition;
        int didHitBoundary;
};

void get_position_next_codepoint(struct TextEdit *edit, struct FileCursor *fc);
void get_position_prev_codepoint(struct TextEdit *edit, struct FileCursor *fc);
void get_position_codepoints_relative(struct TextEdit *edit, struct FileCursor *fc, FILEPOS codepointsDiff);
void get_position_of_line_and_column(struct TextEdit *edit, struct FileCursor *fc, FILEPOS lineNumber, FILEPOS codepointColumn);
void get_position_lines_relative(struct TextEdit *edit, struct FileCursor *fc, FILEPOS linesDiff);
void get_position_line_begin(struct TextEdit *edit, struct FileCursor *fc);
void get_position_line_end(struct TextEdit *edit, struct FileCursor *fc);
void get_position_left(struct TextEdit *edit, struct FileCursor *fc);
void get_position_right(struct TextEdit *edit, struct FileCursor *fc);
void get_position_up(struct TextEdit *edit, struct FileCursor *fc);
void get_position_down(struct TextEdit *edit, struct FileCursor *fc);
void get_position_pageup(struct TextEdit *edit, struct FileCursor *fc);
void get_position_pagedown(struct TextEdit *edit, struct FileCursor *fc);
void get_position_first_line(struct TextEdit *edit, struct FileCursor *fc);
void get_position_last_line(struct TextEdit *edit, struct FileCursor *fc);
void get_position_of_line(struct TextEdit *edit, struct FileCursor *fc, FILEPOS lineNumber);
void get_position_next_match(struct TextEdit *edit, struct FileCursor *fc);

enum MovementKind {
        MOVEMENT_LEFT,
        MOVEMENT_RIGHT,
        MOVEMENT_UP,
        MOVEMENT_DOWN,
        MOVEMENT_NEXT_CODEPOINT,  // like RIGHT but won't stop at end of line
        MOVEMENT_PREVIOUS_CODEPOINT,  // like LEFT but won't stop at beginning of line
        MOVEMENT_NEXT_WORD,
        MOVEMENT_PREVIOUS_WORD,
        MOVEMENT_PAGEUP,
        MOVEMENT_PAGEDOWN,
        MOVEMENT_LINEBEGIN,
        MOVEMENT_LINEEND,
        MOVEMENT_FIRSTLINE,
        MOVEMENT_LASTLINE,
        MOVEMENT_SPECIFICLINE,
        MOVEMENT_SPECIFICLINEANDCOLUMN,
        MOVEMENT_NEXT_MATCH,
};

struct Movement {
        enum MovementKind movementKind;
        FILEPOS pos1;  // when necessary, e.g. SPECIFICLINE
        FILEPOS pos2;  // when necessary, e.g. SPECIFICLINEANDCOLUMN
};

void move_cursor_with_movement(struct TextEdit *edit, struct Movement *movement, int isSelecting);
void delete_with_movement(struct TextEdit *edit, struct Movement *movement);

#define MOVEMENT(...) (&(struct Movement){__VA_ARGS__})
#define MOVE(...) move_cursor_with_movement(edit, MOVEMENT(__VA_ARGS__), isSelecting)
#define DELETE(...) delete_with_movement(edit, MOVEMENT(__VA_ARGS__))

static inline void move_cursor_left(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LEFT); }
static inline void move_cursor_right(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_RIGHT); }
static inline void move_cursor_up(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_UP); }
static inline void move_cursor_down(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_DOWN); }
static inline void move_cursor_to_next_codepoint(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_NEXT_CODEPOINT); }
static inline void move_cursor_to_previous_codepoint(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_PREVIOUS_CODEPOINT); }
static inline void move_cursor_to_next_word(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_NEXT_WORD); }
static inline void move_cursor_to_previous_word(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_PREVIOUS_WORD); }
static inline void move_cursor_up_one_page(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_PAGEUP); }
static inline void move_cursor_down_one_page(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_PAGEDOWN); }
static inline void move_cursor_to_beginning_of_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LINEBEGIN); }
static inline void move_cursor_to_end_of_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LINEEND); }
static inline void move_cursor_to_line_and_column(struct TextEdit *edit, FILEPOS lineNumber, FILEPOS columnCodepoint, int isSelecting) { MOVE(MOVEMENT_SPECIFICLINEANDCOLUMN, lineNumber, columnCodepoint); }
static inline void move_cursor_to_line(struct TextEdit *edit, FILEPOS lineNumber, int isSelecting) { MOVE(MOVEMENT_SPECIFICLINE, lineNumber); }
static inline void move_cursor_to_first_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_FIRSTLINE); }
static inline void move_cursor_to_last_line(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_LASTLINE); }
static inline void move_cursor_to_next_match(struct TextEdit *edit, int isSelecting) { MOVE(MOVEMENT_NEXT_MATCH); }

static inline void delete_left(struct TextEdit *edit) { DELETE(MOVEMENT_LEFT); }
static inline void delete_right(struct TextEdit *edit) { DELETE(MOVEMENT_RIGHT); }
static inline void delete_up(struct TextEdit *edit) { DELETE(MOVEMENT_UP); }
static inline void delete_down(struct TextEdit *edit) { DELETE(MOVEMENT_DOWN); }
static inline void delete_to_next_codepoint(struct TextEdit *edit) { DELETE(MOVEMENT_NEXT_CODEPOINT); }
static inline void delete_to_previous_codepoint(struct TextEdit *edit) { DELETE(MOVEMENT_PREVIOUS_CODEPOINT); }
static inline void delete_to_beginning_of_line(struct TextEdit *edit) { DELETE(MOVEMENT_LINEBEGIN); }
static inline void delete_to_end_of_line(struct TextEdit *edit) { DELETE(MOVEMENT_LINEEND); }
static inline void delete_to_line_and_column(struct TextEdit *edit, FILEPOS lineNumber, FILEPOS columnCodepoint) { DELETE(MOVEMENT_SPECIFICLINEANDCOLUMN, lineNumber, columnCodepoint); }
static inline void delete_to_line(struct TextEdit *edit, FILEPOS lineNumber) { DELETE(MOVEMENT_SPECIFICLINE, lineNumber); }
static inline void delete_to_first_line(struct TextEdit *edit) { DELETE(MOVEMENT_FIRSTLINE); }
static inline void delete_to_last_line(struct TextEdit *edit) { DELETE(MOVEMENT_LASTLINE); }
static inline void delete_to_next_match(struct TextEdit *edit) { DELETE(MOVEMENT_NEXT_MATCH); }
#endif
