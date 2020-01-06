#ifndef ASTEDIT_EDITOR_H_INCLUDED
#define ASTEDIT_EDITOR_H_INCLUDED

#include <astedit/buffers.h>
#include <astedit/regex.h>
#include <astedit/listselect.h>

struct EditorData {
        // buffer selection mode
        int isSelectingBuffer; 
        struct ListSelect bufferSelect;

        int isShowingLineNumbers;
};

DATA struct EditorData globalData;  // for now

#endif
