#ifndef ASTEDIT_VIMODE_H_INCLUDED
#define ASTEDIT_VIMODE_H_INCLUDED

#include <astedit/cmdlinehistory.h>

// XXX forward decl, break cyclic dependency.
struct TextEdit;

/* Optional VI mode. */

enum ViMode {
        VIMODE_NORMAL,
        VIMODE_SELECTING,
        VIMODE_INPUT,
        VIMODE_COMMAND,
        NUM_VIMODE_KINDS,
};

enum ViNormalModeModal {
        VIMODAL_NORMAL,
        VIMODAL_D,
        VIMODAL_G,
};

struct ViCmdline {
        char buf[1024];
        int fill;
        int cursorBytePosition;
        int isAborted;
        int isConfirmed;
        int isNavigatingHistory;
        struct CmdlineHistory history;
};

struct ViState {
        enum ViMode vimodeKind;
        enum ViNormalModeModal modalKind;
        struct ViCmdline cmdline;
};

extern const char *const vimodeKindString[NUM_VIMODE_KINDS];

void setup_vistate(struct ViState *vistate);
void teardown_vistate(struct ViState *vistate);

void interpret_cmdline(struct ViCmdline *cmdline, struct TextEdit *edit);
void clear_ViCmdline(struct ViCmdline *cmdline);
void set_ViCmdline_contents_from_string(struct ViCmdline *cmdline, const char *string, int length);
void insert_codepoint_in_ViCmdline(uint32_t codepoint, struct ViCmdline *cmdline);
void erase_backwards_in_ViCmdline(struct ViCmdline *cmdline);
void erase_forwards_in_ViCmdline(struct ViCmdline *cmdline);

void move_cursor_to_beginning_in_cmdline(struct ViCmdline *cmdline);
void move_cursor_to_end_in_cmdline(struct ViCmdline *cmdline);
void move_cursor_left_in_cmdline(struct ViCmdline *cmdline);
void move_cursor_right_in_cmdline(struct ViCmdline *cmdline);

#endif
