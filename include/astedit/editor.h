#ifndef ASTEDIT_EDITOR_H_INCLUDED
#define ASTEDIT_EDITOR_H_INCLUDED

#include <astedit/buffers.h>
#include <astedit/lineedit.h>
#include <astedit/regex.h>

struct EditorData {
        // buffer selection mode
        int isSelectingBuffer; 
        struct Buffer *selectedBuffer;

        int isSelectingBufferWithSearch;
        struct LineEdit bufferSelectLineEdit;
        struct Regex bufferSelectSearchRegex;
        int bufferSelectSearchRegexValid;

        int isShowingLineNumbers;
};

DATA struct EditorData globalData;  // for now

#endif
