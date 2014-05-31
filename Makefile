PREFIX = /usr/local
BINDIR = ${PREFIX}/bin

X11PREFIX = /usr/X11R6

CFLAGS += -I${X11PREFIX}/include `imlib2-config --cflags` `pkg-config xft --cflags` -g -pedantic -Wall
LDFLAGS += -L${X11PREFIX}/lib -lX11 -lm `imlib2-config --libs` `pkg-config xft --libs`

# Comment these out to disable Xinerama support
CFLAGS += -DXINERAMA
LDFLAGS += -lXext -lXinerama

# Uncomment this for Xinerama debugging
#CFLAGS += -DDEBUG

EXESUFFIX =

SOURCES = skippy.c wm.c dlist.c mainwin.c clientwin.c layout.c focus.c config.c tooltip.c
HEADERS = skippy.h wm.h dlist.h mainwin.h clientwin.h layout.h focus.h config.h tooltip.h

all: skippy${EXESUFFIX}

skippy${EXESUFFIX}: Makefile ${SOURCES} ${HEADERS}
	gcc ${CFLAGS} -o skippy${EXESUFFIX} ${SOURCES} ${LDFLAGS}

clean:
	rm -f skippy${EXESUFFIX}

install:
	install -d ${DESTDIR}${BINDIR}
	install -m 755 skippy$(EXESUFFIX) ${DESTDIR}${BINDIR}/skippy${EXESUFFIX}
