CFLAGS = -g -Wall
CFLAGS += -Wno-switch -Wno-enum-compare
CFLAGS += -D_POSIX_C_SOURE=200809LL

CFLAGS += -Iinclude
CFLAGS += $(shell pkg-config --cflags glfw3)
CFLAGS += $(shell pkg-config --cflags freetype2)
#CFLAGS += $(shell pkg-config --cflags gl)
#CFLAGS += $(shell pkg-config --cflags glu)

LDFLAGS += $(shell pkg-config --libs glfw3)
LDFLAGS += $(shell pkg-config --libs freetype2)
LDFLAGS += $(shell pkg-config --libs gl)
LDFLAGS += $(shell pkg-config --libs glu)
LDFLAGS += -l pthread
LDFLAGS += -l rt  # clock_gettime() and friends

CFILES =  \
src/astedit/bytes.c \
src/astedit/clock-linux.c \
src/astedit/data.c \
src/astedit/draw2d.c \
src/astedit/eventhandling.c \
src/astedit/filereadthread.c \
src/astedit/filesystem-linux.c \
src/astedit/font.c \
src/astedit/font-freetype.c \
src/astedit/gfx-opengl.c \
src/astedit/logging.c \
src/astedit/main.c \
src/astedit/memoryalloc.c \
src/astedit/osthread-linux.c \
src/astedit/stringpool.c \
src/astedit/textedit.c \
src/astedit/textrope.c \
src/astedit/textureatlas.c \
src/astedit/utf8.c \
src/astedit/textropeUTF8decode.c \
src/astedit/window.c \
src/astedit/window-glfw.c \
src/blunt/lex.c \
src/rb3ptr.c

OBJECTS = $(CFILES:%.c=BUILD/%.o)

all: astedit

astedit: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

BUILD/%.o: %.c
	@mkdir -p `dirname $@`
	$(CC) -c $(CFLAGS) -o $@ $^

clean:
	rm -rf astedit BUILD
