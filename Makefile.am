#
# Makefile.am
#
# Copyright (C) 2007 Uday Bondhugula 
# 
#

RM = rm -f
LN = ln -s

if EXTERNAL_ISL
MAY_ISL =
# ISL_INCLUDE and ISL_LIBADD were already defined
else
MAY_ISL = isl
ISL_INCLUDE += -I$(top_srcdir)/isl/include
ISL_LIBADD += $(top_srcdir)/isl/libisl.la
endif

if EXTERNAL_PIPLIB
MAY_PIPLIB =
# PIPLIB_INCLUDE and PIPLIB_LIBADD were already defined
else
MAY_PIPLIB = piplib
endif

if EXTERNAL_POLYLIB
MAY_POLYLIB =
# POLYLIB_INCLUDE and POLYLIB_LIBADD were already defined
else
MAY_POLYLIB = polylib
endif

if EXTERNAL_OSL
MAY_OSL =
# OSL_INCLUDE and OSL_LIBADD were already defined
else
MAY_OSL = openscop
endif

if EXTERNAL_CLOOGISL
MAY_CLOOGISL =
# CLOOGISL_INCLUDE and CLOOGISL_LIBADD were already defined
else
MAY_CLOOGISL = cloog-isl
endif

if EXTERNAL_CLAN
MAY_CLAN =
# CLAN_INCLUDE and CLAN_LIBADD were already defined
else
MAY_CLAN = clan
endif

if EXTERNAL_CANDL
MAY_CANDL =
# CANDL_INCLUDE and CANDL_LIBADD were already defined
else
MAY_CANDL = candl
endif


SUBDIRS = $(MAY_PIPLIB) $(MAY_POLYLIB) $(MAY_ISL) $(MAY_OSL) $(MAY_CLOOGISL) $(MAY_CLAN) $(MAY_CANDL) src

ACLOCAL_AMFLAGS = -I m4

dist-hook:
	rm -rf `find $(distdir)/doc -name CVS`


bin_SCRIPTS = polycc vloog ploog plorc plutune getversion.sh
CLEANFILES: $(bin_SCRIPTS) parsetab.py
EXTRA_DIST = polycc.sh.in  examples test


pkginclude_HEADERS = include/pluto/libpluto.h

polycc: polycc.sh
	rm -f polycc
	echo "#! " $(BASH) > polycc
	cat $(srcdir)/polycc.sh >> polycc
	chmod ugo+x polycc

.PHONY: bin binit

bindir:
	@if [ ! -d bin ];then \
		echo mkdir bin; \
		mkdir bin; \
		fi

binit:
	cp -f src/pluto bin/
	cp -r polycc bin/

pclean:
	$(MAKE) -C src clean

test_libpluto: src/test_libpluto.c
	libtool --mode=link gcc -g src/test_libpluto.c -Lsrc/.libs/ $(ISL_INCLUDE) $(ISL_LIBADD) -I include -lpluto -o test_libpluto

test: .PHO

.PHO: 
	./test.sh
	./test.sh --tile --parallel --unroll
	./test.sh --maxfuse
	./test.sh --nofuse
