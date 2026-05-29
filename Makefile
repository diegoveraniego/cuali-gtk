CC = gcc
CFLAGS = $(shell pkg-config --cflags libadwaita-1 poppler-glib sqlite3) -I./include
LIBS = $(shell pkg-config --libs libadwaita-1 poppler-glib sqlite3) -lm

HAVE_XLSXWRITER := $(shell pkg-config --exists xlsxwriter && echo yes)
ifeq ($(HAVE_XLSXWRITER),yes)
    CFLAGS += -DHAVE_XLSXWRITER $(shell pkg-config --cflags xlsxwriter)
    LIBS += $(shell pkg-config --libs xlsxwriter)
endif

# GUI target
SRC = src/main.c src/database.c src/importer.c src/window.c src/resources.c src/exporter.c src/visualizations.c
OBJ = $(SRC:.c=.o)
TARGET = cuali-gtk

# CLI target — only GLib + SQLite, no GTK/Adwaita/Poppler
CLI_CFLAGS = $(shell pkg-config --cflags glib-2.0 sqlite3) -I./include
CLI_LIBS   = $(shell pkg-config --libs   glib-2.0 sqlite3)
CLI_SRC    = src/cli.c src/database.c
CLI_OBJ    = src/cli.o src/database_cli.o
CLI_TARGET = cuali-cli

all: $(TARGET) $(CLI_TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

# GUI objects use full CFLAGS
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# CLI objects compiled separately without GTK flags
src/cli.o: src/cli.c
	$(CC) $(CLI_CFLAGS) -c -o $@ $<

src/database_cli.o: src/database.c
	$(CC) $(CLI_CFLAGS) -c -o $@ $<

$(CLI_TARGET): src/cli.o src/database_cli.o
	$(CC) -o $@ $^ $(CLI_LIBS)

clean:
	rm -f $(OBJ) $(CLI_OBJ) $(TARGET) $(CLI_TARGET)

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share

install: $(TARGET) $(CLI_TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET)     $(DESTDIR)$(BINDIR)/
	install -m 755 $(CLI_TARGET) $(DESTDIR)$(BINDIR)/
	install -d $(DESTDIR)$(DATADIR)/applications
	install -m 644 org.cuali.CualiGTK.desktop $(DESTDIR)$(DATADIR)/applications/
	install -d $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps
	install -m 644 assets/icon.svg $(DESTDIR)$(DATADIR)/icons/hicolor/scalable/apps/org.cuali.CualiGTK.svg

.PHONY: all clean install
