#include <rb3ptr.h>
#include <astedit/rope.h>


struct Thing {
        char data[16];
        struct Node node;
        struct Thing *next;
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

struct Thing *create_thing(const char *data, int length)
{
        struct Thing *thing = malloc(sizeof *thing);
        memcpy(&thing->data, data, length);
        init_node(&thing->node, data, length);
        
        return thing;
}

struct Thing *get_thing_from_node(struct Node *node)
{
        return (struct Thing *) ((char *)node - offsetof(struct Thing, node));
}

void print_thing(const struct Thing *thing)
{
        printf("%s", thing->data);
}

void print_rope(struct Rope *rope)
{
        printf("Rope now:\n");
        struct Node *node = &rope->firstNode;
        for (;;) {
                node = get_inorder_node(node, RB3_RIGHT);
                if (node == &rope->lastNode)
                        break;
                print_thing(get_thing_from_node(node));
        }
        printf("\n");

        for (int i = 0; i < 100; i++) {
                struct Node *tmp = get_first_intersecting_node(rope, i, 100);
                if (!tmp)
                        break;
                printf("first node at %d is %s\n", i, get_thing_from_node(tmp)->data);
        }
}

int testmain(void)
{
        struct Rope rope;

        init_rope(&rope);

        struct Node *node = &rope.firstNode;
        printf("first Node's length: %d, %d\n", node->ownLength, node->totalLength);

        struct Thing *thing = create_thing("Hello", 6);
        struct Thing *commaThing = create_thing(", ", 3);
        struct Thing *worldThing = create_thing("World!", 7);

        insert_node_next_to_existing(&rope.firstNode,   &thing->node, RB3_RIGHT);
        insert_node_next_to_existing(&thing->node,      &worldThing->node, RB3_RIGHT);
        insert_node_next_to_existing(&worldThing->node, &commaThing->node, RB3_LEFT);

        print_rope(&rope);


        unlink_node(&commaThing->node);

        print_rope(&rope);


        
        return 0;
}



#include <astedit/astedit.h>
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
                                log_write(prefix[flag]);
                                log_write("Ctrl");
                                flag = 1;
                        }
                        if (input.tMousebutton.modifiers & MODIFIER_MOD) {
                                log_write(prefix[flag]);
                                log_write("Mod");
                                flag = 1;
                        }
                        if (input.tMousebutton.modifiers & MODIFIER_SHIFT) {
                                log_write(prefix[flag]);
                                log_write("Shift");
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