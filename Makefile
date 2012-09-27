SHELL = /bin/bash

srcdir = .
top_srcdir = .

prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datarootdir = ${prefix}/share
datadir = ${datarootdir}
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${datarootdir}/info
mandir = ${datarootdir}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/pentagram
pkglibdir = $(libdir)/pentagram
pkgincludedir = $(includedir)/pentagram

top_builddir = .

ACLOCAL = aclocal
AUTOCONF = autoconf
AUTOHEADER = autoheader

INSTALL := /usr/bin/install -c
INSTALL_PROGRAM := ${INSTALL} -s
INSTALL_DATA := ${INSTALL} -m 644
bindir := ${exec_prefix}/bin
datapath := ${datarootdir}/pentagram

CP := cp
CXX := g++
CC := gcc
MKDIR := mkdir -p
RM := rm -f
EXEEXT := 
EMPTY_FILE := /dev/null
CC_FOR_BUILD := gcc
BUILDEXEEXT := 

DEPDIR := .deps
SDL_CFLAGS := -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT
CPPFLAGS :=  -DHAVE_CONFIG_H -DDATA_PATH=\"${datarootdir}/pentagram\"
CXXFLAGS := -g -g -O2 
CFLAGS := -g -g -O2 
LDFLAGS :=  
SYS_LIBS :=  -lz
CON_LIBS := 
GUI_LIBS :=  -L/usr/lib -lSDL -lSDL_ttf -lpng
WARNINGS := -Wall

GIMPTOOL = 
GIMP_INCLUDES = 
GIMP_LIBS = 
GIMP_PLUGIN_PREFIX = 

#GIMP_PLUGIN_MODULES = tools/gimp-plugin
GIMP_PLUGIN_MODULES = 

USE_BUILTIN_DATA := yes

HOST_SYSTEM = 

SYSTEM :=
SYSTEM_MODULES :=

ifeq ($(HOST_SYSTEM),MACOSX)
SYSTEM = $(SYSTEM_MACOSX)
SYSTEM_MODULES := system/macosx
endif

# List of all submodules
# The order is important, be careful if you change it
MODULES := tools/data2c convert convert/u8 convert/crusader misc \
	data filesys filesys/zip \
	tools tools/disasm tools/compile tools/flexpack tools/fold \
	tools/shapeconv $(GIMP_PLUGIN_MODULES)\
	kernel games graphics graphics/fonts graphics/scalers audio \
	audio/midi audio/midi/timidity usecode world world/actors gumps \
	gumps/widgets conf system $(SYSTEM_MODULES) .

# Default target. The other dependencies are added by the module.mk files
all: misc/config.h

# GCC 3.4+ precompiled header
ifeq (no,yes)
PCHEADER := misc/pent_include.h.gch
clean-pch:
	-$(RM) $(PCHEADER)
else
PCHEADER :=
clean-pch:
endif

# create built-in data file if required
ifeq (yes,yes)
filesys/data.o: data/data.h
clean-datah:
	-$(RM) data/data.h
else
clean-datah:
endif

# config.h will be in $(top_builddir)/misc (and so will the precompiled header)
CPPFLAGS += -I$(top_builddir)/misc

# look for include files in each of the modules
CPPFLAGS += $(patsubst %,-I$(top_srcdir)/%,$(MODULES)) -I.

# list of all .deps subdirs
DEPDIRS = $(patsubst %,%/$(top_builddir)/$(DEPDIR),$(MODULES))

# Clean all object files & binaries
clean: clean-pch clean-datah

# Clean *all*
distclean: clean
	-$(RM) -r $(DEPDIRS)
	-$(RM) $(top_builddir)/misc/config.h $(top_builddir)/misc/config.h.stamp

# reset variables
LSRC :=
LGUIPRODUCTS :=
LCONPRODUCTS :=

# Include list of objects
include $(top_srcdir)/objects.mk

# Include additional rules for each module
include $(patsubst %,$(top_srcdir)/%/module.mk,$(MODULES))

# cancel any built-in .cpp->.o rules
%.o: %.cpp

# custom compile rule which calculates dependencies
%.o: %.cpp $(PCHEADER)
	@$(MKDIR) $(*D)/$(DEPDIR)
	$(CXX) -Wp,-MMD,"$(*D)/$(DEPDIR)/$(*F).d",-MQ,"$@",-MP $(WARNINGS) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) -c -o $@ $<

# cancel any built-in .m->.o rules
%.o: %.m

# custom compile rule which calculates dependencies
%.o: %.m $(PCHEADER)
	@$(MKDIR) $(*D)/$(DEPDIR)
	$(CC) -Wp,-MMD,"$(*D)/$(DEPDIR)/$(*F).d",-MQ,"$@",-MP $(WARNINGS) $(CPPFLAGS) $(CFLAGS) $(SDL_CFLAGS) -c -o $@ $<

# custom compile rule for the precompiled header
%.h.gch: %.h
	@$(MKDIR) $(*D)/$(DEPDIR)
	$(CXX) -Wp,-MMD,"$(*D)/$(DEPDIR)/$(*F).h.gch.d",-MQ,"$@",-MP $(WARNINGS) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) -x c++-header -c -o $@ $<


# rule to rebuild Makefile if necessary
Makefile: $(srcdir)/Makefile.in $(top_builddir)/config.status
	cd $(top_builddir) && \
	  CONFIG_HEADERS= CONFIG_LINKS= CONFIG_FILES=$@ \
	  $(SHELL) ./config.status

# rules to rebuild config.h if necessary
misc/config.h: misc/config.h.stamp

misc/config.h.stamp: $(srcdir)/misc/config.h.in $(top_builddir)/config.status
	@$(RM) misc/config.h.stamp
	cd $(top_builddir) && \
	  CONFIG_HEADERS=misc/config.h CONFIG_LINKS= CONFIG_FILES= \
	  $(SHELL) ./config.status

# including config.h.stamp lets make rescan all dependencies if
# config.h may have been changed.
-include misc/config.h.stamp



# rules to rebuild configure, config.status, aclocal.m4, config.h.in
$(top_builddir)/config.status: $(srcdir)/configure
	$(SHELL) ./config.status --recheck
$(srcdir)/configure: $(srcdir)/configure.ac $(top_srcdir)/aclocal.m4
	cd $(srcdir) && $(AUTOCONF)
$(top_srcdir)/aclocal.m4: $(srcdir)/configure.ac $(srcdir)/acinclude.m4
	cd $(srcdir) && $(ACLOCAL)
$(srcdir)/misc/config.h.in: $(top_srcdir)/configure.ac $(top_srcdir)/aclocal.m4
	cd $(top_srcdir) && $(AUTOHEADER)
	touch $(srcdir)/misc/config.h.in

.PHONY: all clean distclean clean-pch clean-datah
