CFLAGS = -g -Wall -D_POSIX_C_SOURE=200809LL -Iinclude
LDFLAGS = -lglfw -lfreetype -lGL -lGLU

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
