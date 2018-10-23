# ##########################################################################
# LZ4 programs - Makefile
# Copyright (C) Yann Collet 2011-2017
#
# This Makefile is validated for Linux, macOS, *BSD, Hurd, Solaris, MSYS2 targets
#
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - LZ4 homepage : http://www.lz4.org
#  - LZ4 source repository : https://github.com/Cyan4973/lz4
# ##########################################################################
# lz4 : Command Line Utility, supporting gzip-like arguments
# lz4c  : CLU, supporting also legacy lz4demo arguments
# lz4c32: Same as lz4c, but forced to compile in 32-bits mode
# ##########################################################################

# Version numbers
LZ4DIR   := ../lib
LIBVER_SRC := $(LZ4DIR)/lz4.h
LIBVER_MAJOR_SCRIPT:=`sed -n '/define LZ4_VERSION_MAJOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_MINOR_SCRIPT:=`sed -n '/define LZ4_VERSION_MINOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_PATCH_SCRIPT:=`sed -n '/define LZ4_VERSION_RELEASE/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_SCRIPT:= $(LIBVER_MAJOR_SCRIPT).$(LIBVER_MINOR_SCRIPT).$(LIBVER_PATCH_SCRIPT)
LIBVER_MAJOR := $(shell echo $(LIBVER_MAJOR_SCRIPT))
LIBVER_MINOR := $(shell echo $(LIBVER_MINOR_SCRIPT))
LIBVER_PATCH := $(shell echo $(LIBVER_PATCH_SCRIPT))
LIBVER   := $(shell echo $(LIBVER_SCRIPT))

SRCFILES := $(sort $(wildcard $(LZ4DIR)/*.c) $(wildcard *.c))
OBJFILES := $(SRCFILES:.c=.o)

CPPFLAGS += -I$(LZ4DIR) -DXXH_NAMESPACE=LZ4_
CFLAGS   ?= -O3
DEBUGFLAGS:=-Wall -Wextra -Wundef -Wcast-qual -Wcast-align -Wshadow \
            -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes \
            -Wpointer-arith -Wstrict-aliasing=1
CFLAGS   += $(DEBUGFLAGS) $(MOREFLAGS)
FLAGS     = $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

LZ4_VERSION=$(LIBVER)
MD2ROFF   = ronn
MD2ROFF_FLAGS = --roff --warnings --manual="User Commands" --organization="lz4 $(LZ4_VERSION)"


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT  :=.exe
VOID := nul
else
EXT  :=
VOID := /dev/null
endif



default: lz4-release

all: lz4 lz4c

all32: CFLAGS+=-m32
all32: all

lz4: $(OBJFILES)
	$(CC) $(FLAGS) $^ -o $@$(EXT)

lz4-release: DEBUGFLAGS=
lz4-release: lz4

lz4c: lz4
	ln -s lz4 lz4c

lz4c32: CFLAGS += -m32
lz4c32 : $(SRCFILES)
	$(CC) $(FLAGS) $^ -o $@$(EXT)

lz4.1: lz4.1.md
	cat $^ | $(MD2ROFF) $(MD2ROFF_FLAGS) | sed -n '/^\.\\\".*/!p' > $@

man: lz4.1

clean-man:
	rm lz4.1

preview-man: clean-man man
	man ./lz4.1

clean:
	@$(MAKE) -C $(LZ4DIR) $@ > $(VOID)
	@$(RM) core *.o *.test tmp* \
           lz4$(EXT) lz4c$(EXT) lz4c32$(EXT) unlz4 lz4cat
	@echo Cleaning completed


#-----------------------------------------------------------------------------
# make install is validated only for Linux, OSX, BSD, Hurd and Solaris targets
#-----------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))

unlz4: lz4
	ln -s lz4 unlz4

lz4cat: lz4
	ln -s lz4 lz4cat

DESTDIR     ?=
# directory variables : GNU conventions prefer lowercase
# see https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html
# support both lower and uppercase (BSD), use uppercase in script
prefix      ?= /usr/local
PREFIX      ?= $(prefix)
exec_prefix ?= $(PREFIX)
bindir      ?= $(exec_prefix)/bin
BINDIR      ?= $(bindir)
datarootdir ?= $(PREFIX)/share
mandir      ?= $(datarootdir)/man
man1dir     ?= $(mandir)/man1

ifneq (,$(filter $(shell uname),OpenBSD FreeBSD NetBSD DragonFly SunOS))
MANDIR  ?= $(PREFIX)/man/man1
else
MANDIR  ?= $(man1dir)
endif

ifneq (,$(filter $(shell uname),SunOS))
INSTALL ?= ginstall
else
INSTALL ?= install
endif

INSTALL_PROGRAM ?= $(INSTALL) -m 755
INSTALL_DATA    ?= $(INSTALL) -m 644


install: lz4
	@echo Installing binaries
	@$(INSTALL) -d -m 755 $(DESTDIR)$(BINDIR)/ $(DESTDIR)$(MANDIR)/
	@$(INSTALL_PROGRAM) lz4 $(DESTDIR)$(BINDIR)/lz4
	@ln -sf lz4 $(DESTDIR)$(BINDIR)/lz4c
	@ln -sf lz4 $(DESTDIR)$(BINDIR)/lz4cat
	@ln -sf lz4 $(DESTDIR)$(BINDIR)/unlz4
	@echo Installing man pages
	@$(INSTALL_DATA) lz4.1 $(DESTDIR)$(MANDIR)/lz4.1
	@ln -sf lz4.1 $(DESTDIR)$(MANDIR)/lz4c.1
	@ln -sf lz4.1 $(DESTDIR)$(MANDIR)/lz4cat.1
	@ln -sf lz4.1 $(DESTDIR)$(MANDIR)/unlz4.1
	@echo lz4 installation completed

uninstall:
	@$(RM) $(DESTDIR)$(BINDIR)/lz4cat
	@$(RM) $(DESTDIR)$(BINDIR)/unlz4
	@$(RM) $(DESTDIR)$(BINDIR)/lz4
	@$(RM) $(DESTDIR)$(BINDIR)/lz4c
	@$(RM) $(DESTDIR)$(MANDIR)/lz4.1
	@$(RM) $(DESTDIR)$(MANDIR)/lz4c.1
	@$(RM) $(DESTDIR)$(MANDIR)/lz4cat.1
	@$(RM) $(DESTDIR)$(MANDIR)/unlz4.1
	@echo lz4 programs successfully uninstalled

endif
