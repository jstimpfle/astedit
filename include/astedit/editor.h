#ifndef ASTEDIT_EDITOR_H_INCLUDED
#define ASTEDIT_EDITOR_H_INCLUDED

#include <astedit/buffers.h>

struct EditorData {
        // buffer selection mode
        int isSelectingBuffer; 
        struct Buffer *selectedBuffer;

        int isShowingLineNumbers;
};

DATA struct EditorData globalData;  // for now

#endif
