# ##########################################################################
# LZ4 examples - Makefile
# Copyright (C) Yann Collet 2011-2020
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
#  - LZ4 source repository : https://github.com/lz4/lz4
#  - LZ4 forum froup : https://groups.google.com/forum/#!forum/lz4c
# ##########################################################################
# This makefile compile and test
# example programs, using (mostly) LZ4 streaming library,
# kindly provided by Takayuki Matsuoka
# ##########################################################################

LIBDIR    := ../lib

CPPFLAGS  += -I$(LIBDIR)
USERCFLAGS:= $(CFLAGS)
WFLAGS     = -std=gnu99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes -Wc++-compat
CFLAGS     = $(WFLAGS) -O2 $(USERCFLAGS)

TESTFILE   = Makefile
SLIBLZ4   := $(LIBDIR)/liblz4.a
LZ4DIR     = ../programs
LZ4        = $(LZ4DIR)/lz4

default: all

$(SLIBLZ4): $(LIBDIR)/lz4.c $(LIBDIR)/lz4hc.c $(LIBDIR)/lz4frame.c $(LIBDIR)/lz4.h $(LIBDIR)/lz4hc.h $(LIBDIR)/lz4frame.h $(LIBDIR)/lz4frame_static.h
	$(MAKE) -j -C $(LIBDIR) liblz4.a

ALL = print_version \
	  simple_buffer \
	  frameCompress \
	  fileCompress \
      blockStreaming_doubleBuffer \
	  blockStreaming_ringBuffer \
	  streamingHC_ringBuffer \
      blockStreaming_lineByLine \
	  dictionaryRandomAccess \
	  bench_functions

.PHONY: all
all: $(ALL)

$(ALL): $(SLIBLZ4)

.PHONY:$(LZ4)
$(LZ4) :
	$(MAKE) -j -C $(LZ4DIR) lz4

.PHONY: test
test : all $(LZ4)
	@echo "\n=== Print Version ==="
	./print_version$(EXT)
	@echo "\n=== Simple compression example ==="
	./simple_buffer$(EXT)
	@echo "\n=== Double-buffer ==="
	./blockStreaming_doubleBuffer$(EXT) $(TESTFILE)
	@echo "\n=== Ring Buffer ==="
	./blockStreaming_ringBuffer$(EXT)   $(TESTFILE)
	@echo "\n=== Ring Buffer + LZ4 HC ==="
	./streamingHC_ringBuffer$(EXT) $(TESTFILE)
	@echo "\n=== Compress line by line ==="
	./blockStreaming_lineByLine$(EXT) $(TESTFILE)
	@echo "\n=== Dictionary Random Access ==="
	./dictionaryRandomAccess$(EXT) $(TESTFILE) $(TESTFILE) 1100 1400
	@echo "\n=== Frame compression ==="
	./frameCompress$(EXT) $(TESTFILE)
	$(LZ4) -vt $(TESTFILE).lz4
	@echo "\n=== file compression ==="
	./fileCompress$(EXT) $(TESTFILE)
	$(LZ4) -vt $(TESTFILE).lz4
	@echo "\n=== Q&D benchmark ==="
	./bench_functions$(EXT) 10000

.PHONY: clean
clean:
	@$(RM) core *.o *.dec *-0 *-9 *-8192 *.lz4s *.lz4 $(ALL)
	@echo Cleaning completed
