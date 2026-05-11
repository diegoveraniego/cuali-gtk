CC = gcc
CFLAGS = $(shell pkg-config --cflags libadwaita-1 poppler-glib sqlite3) -I./include
LIBS = $(shell pkg-config --libs libadwaita-1 poppler-glib sqlite3)

SRC = src/main.c src/database.c src/importer.c src/window.c
OBJ = $(SRC:.c=.o)
TARGET = cuali-gtk

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
