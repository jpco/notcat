# Basic configuration.

CC = gcc
INSTALL = install
MKDIR_P = mkdir -p

bindir = /usr/local/bin
mandir = /usr/local/man
srcdir = .

# End basic configuration.

CSRC = fmt.c main.c buffer.c run.c client.c capabilities.c parse.c
HSRC = notcat.h

CFLAGS = -Wall -Werror -O0 -ggdb

DEPS     = gio-2.0 gobject-2.0 glib-2.0
LIBS     = $(shell pkg-config --libs ${DEPS})
INCLUDES = $(shell pkg-config --cflags ${DEPS})

notcat 		: ${CSRC} libnotlib.a
	${CC} -o notcat ${DEFINES} ${CFLAGS} ${CSRC} -L./notlib -lnotlib ${LIBS} ${INCLUDES}

libnotlib.a	:
	$(MAKE) static -C notlib DEFINES=-DNL_REMOTE_ACTIONS=1

install		: notcat
	$(MKDIR_P) $(bindir)
	$(INSTALL) -s $(srcdir)/notcat $(bindir)
	$(MKDIR_P) $(mandir)/man1
	$(INSTALL) $(srcdir)/notcat.1 $(mandir)/man1

clean		:
	$(MAKE) clean -C notlib
	rm *.o notcat
