#include <astedit/astedit.h>
#include <astedit/bytes.h>
#include <astedit/memory.h>
#include <astedit/cmdlinehistory.h>

struct RememberedCmdline *go_to_previous_cmdline(struct CmdlineHistory *history)
{
        if (history->iter == history->oldest)
                return NULL;
        else if (history->iter->prev == NULL)
                return NULL;
        else
                history->iter = history->iter->prev;
        return history->iter;
}

struct RememberedCmdline *go_to_next_cmdline(struct CmdlineHistory *history)
{
        if (history->iter == NULL)
                return NULL;
        else if (history->iter->next == NULL)
                return NULL;
        else {
                history->iter = history->iter->next;
                return history->iter;
        }
}

struct RememberedCmdline *go_to_most_recent(struct CmdlineHistory *history)
{
        history->iter = history->newest;
        return history->iter;
}

void add_to_cmdline_history(struct CmdlineHistory *history, const char *cmdline, int length)
{
        struct RememberedCmdline *newItem;
        ALLOC_MEMORY(&newItem, 1);
        ALLOC_MEMORY(&newItem->cmdline, length + 1);
        copy_string_and_zeroterminate(newItem->cmdline, cmdline, length);
        newItem->length = length;
        newItem->prev = history->newest;
        newItem->next = NULL;
        if (history->newest)
                history->newest->next = newItem;
        if (history->oldest == NULL)
                history->oldest = newItem;
        history->newest = newItem;
}
