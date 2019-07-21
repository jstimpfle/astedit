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
bytes.c \
clock-linux.c \
data.c \
draw2d.c \
font.c \
font-freetype.c \
gfx-opengl.c \
logging.c \
main.c \
memoryalloc.c \
rb3ptr.c \
stringpool.c \
textedit.c \
textrope.c \
textureatlas.c \
utf8.c \
window.c \
window-glfw.c

OBJECTS = $(CFILES:=.o)

all: astedit

astedit: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: src/%
	$(CC) -c $(CFLAGS) -o $@ $^

clean:
	rm -f astedit $(OBJECTS)
