#include <astedit/astedit.h>
#include <astedit/draw2d.h>
#include <astedit/window.h>
#include <astedit/clock.h>
#include <astedit/logging.h>
#include <astedit/font.h>
#include <astedit/gfx.h>
#include <astedit/textedit.h>


static struct TextEdit globalTextEdit;

void handle_events(void)
{

        for (struct Input input;
                look_input(&input);
                consume_input())
        {
                if (input.inputKind == INPUT_WINDOWRESIZE)
                        log_postf("Window size is now %d %d",
                                input.tWindowresize.width,
                                input.tWindowresize.height);
                else if (input.inputKind == INPUT_KEY) {
                        if (input.tKey.keyKind == KEY_ESCAPE) {
                                shouldWindowClose = 1;
                        }
                        else {
                                process_input_in_textEdit(&input, &globalTextEdit);
                        }
                }
                else if (input.inputKind == INPUT_MOUSEBUTTON) {
                        log_begin();
                        log_writef("%s mouse button %d",
                                input.tMousebutton.mousebuttonEventKind == MOUSEBUTTONEVENT_PRESS ? "Press" : "Release",
                                input.tMousebutton.mousebuttonKind);

                        int flag = 0;
                        const char *prefix[2] = { " with ", "+" };
                        if (input.tMousebutton.modifiers & MODIFIER_CONTROL) {
                                log_write_cstring(prefix[flag]);
                                log_write_cstring("Ctrl");
                                flag = 1;
                        }
                        if (input.tMousebutton.modifiers & MODIFIER_MOD) {
                                log_write_cstring(prefix[flag]);
                                log_write_cstring("Mod");
                                flag = 1;
                        }
                        if (input.tMousebutton.modifiers & MODIFIER_SHIFT) {
                                log_write_cstring(prefix[flag]);
                                log_write_cstring("Shift");
                                flag = 1;
                        }
                        log_end();
                }
        }
}

int main(void)
{
        setup_window();
        setup_fonts();
        setup_gfx();

        init_TextEdit(&globalTextEdit);

        //textedit_test_init(&globalTextEdit);

        while (!shouldWindowClose) {
                wait_for_events();
                handle_events();

                testdraw(&globalTextEdit);

                sleep_milliseconds(13);
        }

        exit_TextEdit(&globalTextEdit);

        teardown_gfx();
        teardown_fonts();
        teardown_window();
        return 0;
}
