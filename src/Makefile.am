# Pluto
#
# Copyright (C) Uday Bondhugula 2007
#

SUBDIRS = 

MAINTAINERCLEANFILES = Makefile.in


if EXTERNAL_ISL
# ISL_INCLUDE, ISL_LIBADD were already defined
else
ISL_INCLUDE += -I$(top_srcdir)/isl/include
ISL_LIBADD += $(top_srcdir)/isl/libisl.la
endif

bin_PROGRAMS = pluto

pluto_SOURCES = constraints.c constraints_polylib.c constraints_isl.c math_support.c ddg.c transforms.c pluto.c post_transform.c framework.c pluto_codegen_if.c polyloop.c ast_transform.c main.c constraints.h math_support.h pluto.h program.c program.h tile.c unit_tests.c
pluto_CPPFLAGS = -O3 -funroll-loops -fopenmp \
   -DSCOPLIB_INT_T_IS_MP -DPIP_WIDTH_MP -DCLOOG_INT_GMP \
   -I../include \
   $(PIPLIB_INCLUDE) \
   $(CLAN_INCLUDE) \
   $(ISL_INCLUDE) \
   $(CLOOGISL_INCLUDE) \
   $(OSL_INCLUDE) \
   $(CANDL_INCLUDE) \
   $(POLYLIB_INCLUDE)

pluto_LDFLAGS = -static -fopenmp
pluto_LDADD =  \
   $(PIPLIB_LIBADD) \
   $(OSL_LIBADD) \
   $(CLAN_LIBADD) \
   $(CLOOGISL_LIBADD) \
   $(ISL_LIBADD) \
   $(POLYLIB_LIBADD) \
   $(CANDL_LIBADD) \
   -lm

lib_LTLIBRARIES = libpluto.la

# libpluto doesn't use polylib-based functions
libpluto_la_SOURCES = constraints.c constraints_isl.c math_support.c ddg.c transforms.c pluto.c post_transform.c framework.c polyloop.c libpluto.c constraints.h math_support.h pluto.h program.c program.h tile.c
libpluto_la_CPPFLAGS = -O3 -funroll-loops -fopenmp \
   -DSCOPLIB_INT_T_IS_MP -DPIP_WIDTH_MP -DCLOOG_INT_GMP \
   -I../include \
   $(PIPLIB_INCLUDE) \
   $(CLAN_INCLUDE) \
   $(OSL_INCLUDE) \
   $(CANDL_INCLUDE) \
   $(CLOOGISL_INCLUDE) \
   $(ISL_INCLUDE)

# libpluto doesn't use polylib
libpluto_la_LIBADD = \
   $(PIPLIB_LIBADD) \
   $(OSL_LIBADD) \
   $(CLAN_LIBADD) \
   $(CANDL_LIBADD) \
   $(ISL_LIBADD) \
   -lm -lgomp
