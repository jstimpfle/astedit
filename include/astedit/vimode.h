#ifndef ASTEDIT_VIMODE_H_INCLUDED
#define ASTEDIT_VIMODE_H_INCLUDED

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
};

struct ViState {
        enum ViMode vimodeKind;
        enum ViNormalModeModal modalKind;
        struct ViCmdline cmdline;
};

extern const char *const vimodeKindString[NUM_VIMODE_KINDS];



void interpret_cmdline(struct ViCmdline *cmdline, struct TextEdit *edit);
void reset_ViCmdline(struct ViCmdline *cmdline);
void insert_codepoint_in_ViCmdline(uint32_t codepoint, struct ViCmdline *cmdline);
void erase_backwards_in_ViCmdline(struct ViCmdline *cmdline);
void erase_forwards_in_ViCmdline(struct ViCmdline *cmdline);

#endif