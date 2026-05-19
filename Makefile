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

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 org.cuali.CualiGTK.desktop $(DESTDIR)$(DATADIR)/applications/
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps
	install -m 644 assets/icon.svg $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/org.cuali.CualiGTK.svg

.PHONY: all clean install
