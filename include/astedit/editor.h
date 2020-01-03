#ifndef ASTEDIT_EDITOR_H_INCLUDED
#define ASTEDIT_EDITOR_H_INCLUDED

#include <astedit/buffers.h>
#include <astedit/lineedit.h>

struct EditorData {
        // buffer selection mode
        int isSelectingBuffer; 
        struct Buffer *selectedBuffer;

        int isSelectingBufferWithSearch;
        struct LineEdit bufferSelectLineEdit;

        int isShowingLineNumbers;
};

DATA struct EditorData globalData;  // for now

#endif
