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

static struct Timer redrawTimer;
static struct Timer mainloopTimer;
static struct Timer waitEventsTimer;
static struct Timer handleEventsTimer;

static void handle_events(void)
{
        for (struct Input input;
                look_input(&input);
                consume_input())
        {
                handle_input(&input, &globalTextEdit);
        }

        //XXX: "TIMETICK" event
        update_textedit(&globalTextEdit);
}

void mainloop(void)
{
        start_timer(&mainloopTimer);

        update_clock();

        start_timer(&waitEventsTimer);
        wait_for_events();
        stop_timer(&waitEventsTimer);

        start_timer(&handleEventsTimer);
        handle_events();
        stop_timer(&handleEventsTimer);

        start_timer(&redrawTimer);
        testdraw(&globalTextEdit);
        stop_timer(&redrawTimer);

        flush_gfx();
        stop_timer(&mainloopTimer);

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
        setup_timers();
        setup_window();
        setup_fonts();
        setup_gfx();

        init_TextEdit(&globalTextEdit);

        if (argc == 2)
                textedit_test_init(&globalTextEdit, argv[1]);
        globalTextEdit.isVimodeActive = 1;

        while (!shouldWindowClose)
                mainloop();

        exit_TextEdit(&globalTextEdit);

        teardown_gfx();
        teardown_fonts();
        teardown_window();
        return 0;
}
