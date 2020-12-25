SHELL =		/bin/sh

all:
	@if [ ! -f src/Makefile ] ; then \
		echo '*** Makefile not found in src/.  Did you forget to ./configure ? ***' ; \
	else \
		${MAKE} build ; \
	fi

build:
	cd src ; ${MAKE} all
	cd doc ; ${MAKE} all

html:
	cd doc ; ${MAKE} html

pdf:
	cd doc ; ${MAKE} pdf

strip:
	cd src ; ${MAKE} ${MAKEFLAGS} strip

install:
	cd src ; ${MAKE} ${MAKEFLAGS} install
	cd doc ; ${MAKE} ${MAKEFLAGS} install

uninstall:
	cd src ; ${MAKE} ${MAKEFLAGS} uninstall
	cd doc ; ${MAKE} ${MAKEFLAGS} uninstall

clean:
	rm -fr autom4te.cache
	rm -f config.log config.status
	cd src ; ${MAKE} clean
	cd doc ; ${MAKE} clean

distclean:
	${MAKE} clean
	cd src ; ${MAKE} distclean
	cd doc ; ${MAKE} distclean
	cd src ; rm -f Makefile config.h version.h
	cd doc ; rm -f doc/Makefile doc/version.texi
	rm -f configure *~ *#
