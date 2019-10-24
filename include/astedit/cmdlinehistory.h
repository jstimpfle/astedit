#ifndef CMDLINEHISTORY_H_INCLUDED
#define CMDLINEHISTORY_H_INCLUDED

struct RememberedCmdline {
        struct RememberedCmdline *prev;
        struct RememberedCmdline *next;
        char *cmdline;
        int length;
};

struct CmdlineHistory {
        struct RememberedCmdline *iter; /* pointer to currently visited item (Up/Down cursor keys) */
        struct RememberedCmdline *oldest;
        struct RememberedCmdline *newest;
};

struct RememberedCmdline *go_to_previous_cmdline(struct CmdlineHistory *history);
struct RememberedCmdline *go_to_next_cmdline(struct CmdlineHistory *history);
struct RememberedCmdline *go_to_most_recent(struct CmdlineHistory *history);
void add_to_cmdline_history(struct CmdlineHistory *history, const char *cmdline, int length);

#endif
