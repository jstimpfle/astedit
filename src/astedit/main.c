#include <astedit/astedit.h>
#include <astedit/draw2d.h>
#include <astedit/window.h>
#include <astedit/clock.h>
#include <astedit/logging.h>
#include <astedit/font.h>
#include <astedit/gfx.h>
#include <astedit/textedit.h>
#include <astedit/eventhandling.h>


static struct TextEdit globalTextEdit;

static Timer *keyinputTimer;
static Timer *redrawTimer;
static Timer *mainloopTimer;
static Timer *waitEventsTimer;
static Timer *handleEventsTimer;



void handle_events(void)
{
        for (struct Input input;
                look_input(&input);
                consume_input())
        {
                if (input.inputKind == INPUT_WINDOWRESIZE) {
                        /*log_postf("Window size is now %d %d",
                                input.tWindowresize.width,
                                input.tWindowresize.height);
                                */
                }
                else if (input.inputKind == INPUT_KEY) {
                        if (input.data.tKey.keyKind == KEY_ESCAPE) {
                                shouldWindowClose = 1;
                        }
                        else {
                                start_timer(keyinputTimer);
                                process_input_in_TextEdit(&input, &globalTextEdit);
                                stop_timer(keyinputTimer);
                                report_timer(keyinputTimer, "Time spent in editing operation");
                        }
                }
                else if (input.inputKind == INPUT_MOUSEBUTTON) {
                        log_begin();
                        log_writef("%s mouse button %d",
                                input.data.tMousebutton.mousebuttonEventKind == MOUSEBUTTONEVENT_PRESS ? "Press" : "Release",
                                input.data.tMousebutton.mousebuttonKind);

                        int flag = 0;
                        const char *prefix[2] = { " with ", "+" };
                        if (input.data.tMousebutton.modifiers & MODIFIER_CONTROL) {
                                log_write_cstring(prefix[flag]);
                                log_write_cstring("Ctrl");
                                flag = 1;
                        }
                        if (input.data.tMousebutton.modifiers & MODIFIER_MOD) {
                                log_write_cstring(prefix[flag]);
                                log_write_cstring("Mod");
                                flag = 1;
                        }
                        if (input.data.tMousebutton.modifiers & MODIFIER_SHIFT) {
                                log_write_cstring(prefix[flag]);
                                log_write_cstring("Shift");
                                flag = 1;
                        }
                        log_end();
                }
        }
}

void mainloop(void)
{
        start_timer(mainloopTimer);

        update_clock();

        start_timer(waitEventsTimer);
        wait_for_events();
        stop_timer(waitEventsTimer);

        start_timer(handleEventsTimer);
        handle_events();
        stop_timer(handleEventsTimer);

        start_timer(redrawTimer);
        testdraw(&globalTextEdit);
        stop_timer(redrawTimer);

        flush_gfx();
        stop_timer(mainloopTimer);

        /*
        report_timer(waitEventsTimer, "Wait for events");
        report_timer(handleEventsTimer, "Handle events");
        report_timer(redrawTimer, "Redraw frame");
        report_timer(mainloopTimer, "Main loop");
        */

        swap_buffers();
        sleep_milliseconds(13);
}

int main(int argc, const char **argv)
{
        setup_window();
        setup_fonts();
        setup_gfx();

        init_TextEdit(&globalTextEdit);

        keyinputTimer = create_timer();
        redrawTimer = create_timer();
        mainloopTimer = create_timer();
        waitEventsTimer = create_timer();
        handleEventsTimer = create_timer();

        if (argc == 2)
                textedit_test_init(&globalTextEdit, argv[1]);

        while (!shouldWindowClose)
                mainloop();

        destroy_timer(keyinputTimer);
        destroy_timer(redrawTimer);
        destroy_timer(mainloopTimer);
        destroy_timer(waitEventsTimer);
        destroy_timer(handleEventsTimer);

        exit_TextEdit(&globalTextEdit);

        teardown_gfx();
        teardown_fonts();
        teardown_window();
        return 0;
}
