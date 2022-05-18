VERSION = git-20131005

PREFIX    = /usr/local
MANPREFIX = $(PREFIX)/share/man

PKG_CONFIG = pkg-config

CC      = gcc
CFLAGS  = -std=c99 -Wall -pedantic -g -I$(PREFIX)/include \
		  -I -DHAVE_GIFLIB \
		  `$(PKG_CONFIG) --cflags fontconfig` \
		  `$(PKG_CONFIG) --cflags freetype2`
LDFLAGS = -L$(PREFIX)/lib
LIBS    = -lX11 -lImlib2 -lgif -larchive -lXft \
		  `$(PKG_CONFIG) --libs fontconfig` \
		  `$(PKG_CONFIG) --libs freetype2`

SRC = commands.c exif.c image.c main.c options.c thumbs.c util.c window.c archive.c
OBJ = $(SRC:.c=.o)

all: sxiv

$(OBJ): Makefile config.h

.c.o:
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -c -o $@ $<

config.h:
	cp config.def.h $@

sxiv:	$(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ) sxiv

debug: CFLAGS += -DDEBUG -g -O0 -fsanitize=address -fsanitize=leak
debug: LDFLAGS += -fsanitize=address -fsanitize=leak
debug: sxiv

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp sxiv $(DESTDIR)$(PREFIX)/bin/sxiv-manga
	chmod 755 $(DESTDIR)$(PREFIX)/bin/sxiv-manga
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s!PREFIX!$(PREFIX)!g; s!VERSION!$(VERSION)!g" sxiv.1 > $(DESTDIR)$(MANPREFIX)/man1/sxiv.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/sxiv.1
	mkdir -p $(DESTDIR)$(PREFIX)/share/sxiv/exec
	cp image-info $(DESTDIR)$(PREFIX)/share/sxiv/exec/image-info
	chmod 755 $(DESTDIR)$(PREFIX)/share/sxiv/exec/image-info

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/sxiv
	rm -f $(DESTDIR)$(MANPREFIX)/man1/sxiv.1
	rm -rf $(DESTDIR)$(PREFIX)/share/sxiv
