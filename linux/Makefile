CFLAGS = -g -Wall
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

CFILES =  \
src/astedit/bytes.c \
src/astedit/clock-linux.c \
src/astedit/data.c \
src/astedit/draw2d.c \
src/astedit/font.c \
src/astedit/font-freetype.c \
src/astedit/gfx-opengl.c \
src/astedit/logging.c \
src/astedit/main.c \
src/astedit/memoryalloc.c \
src/astedit/stringpool.c \
src/astedit/textedit.c \
src/astedit/textrope.c \
src/astedit/textureatlas.c \
src/astedit/utf8.c \
src/astedit/window.c \
src/astedit/window-glfw.c \
src/rb3ptr.c

OBJECTS = $(CFILES:.c=.o)

all: astedit

astedit: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $^

clean:
	rm -f astedit $(OBJECTS)
