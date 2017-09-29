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
#  - LZ4 source repository : https://github.com/lz4/lz4
# ##########################################################################
# fuzzer  : Test tool, to check lz4 integrity on target platform
# frametest  : Test tool, to check lz4frame integrity on target platform
# fullbench  : Precisely measure speed for each LZ4 function variant
# datagen : generates synthetic data samples for tests & benchmarks
# ##########################################################################

LZ4DIR  := ../lib
PRGDIR  := ../programs
TESTDIR := versionsTest
PYTHON  ?= python3

CFLAGS  ?= -O3 # can select custom optimization flags. For example : CFLAGS=-O2 make
CFLAGS  += -g -Wall -Wextra -Wundef -Wcast-qual -Wcast-align -Wshadow \
           -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes \
           -Wpointer-arith -Wstrict-aliasing=1
CFLAGS  += $(MOREFLAGS)
CPPFLAGS:= -I$(LZ4DIR) -I$(PRGDIR) -DXXH_NAMESPACE=LZ4_
FLAGS    = $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT  =.exe
VOID = nul
else
EXT  =
VOID = /dev/null
endif
LZ4     := $(PRGDIR)/lz4$(EXT)


# Default test parameters
TEST_FILES   := COPYING
FUZZER_TIME  := -T3mn
NB_LOOPS     ?= -i1


default: all

all: fullbench fuzzer frametest datagen

all32: CFLAGS+=-m32
all32: all

lz4:
	$(MAKE) -C $(PRGDIR) $@ CFLAGS="$(CFLAGS)"

lz4c unlz4 lz4cat: lz4
	ln -sf $(LZ4) $(PRGDIR)/$@

lz4c32:   # create a 32-bits version for 32/64 interop tests
	$(MAKE) -C $(PRGDIR) $@ CFLAGS="-m32 $(CFLAGS)"

fullbench  : $(LZ4DIR)/lz4.o $(LZ4DIR)/lz4hc.o $(LZ4DIR)/lz4frame.o $(LZ4DIR)/xxhash.o fullbench.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)

fullbench-lib: fullbench.c $(LZ4DIR)/xxhash.c
	$(MAKE) -C $(LZ4DIR) liblz4.a
	$(CC) $(FLAGS) $^ -o $@$(EXT) $(LZ4DIR)/liblz4.a

fullbench-dll: fullbench.c $(LZ4DIR)/xxhash.c
	$(MAKE) -C $(LZ4DIR) liblz4
	$(CC) $(FLAGS) $^ -o $@$(EXT) -DLZ4_DLL_IMPORT=1 $(LZ4DIR)/dll/liblz4.dll

fuzzer  : $(LZ4DIR)/lz4.o $(LZ4DIR)/lz4hc.o $(LZ4DIR)/xxhash.o fuzzer.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)

frametest: $(LZ4DIR)/lz4frame.o $(LZ4DIR)/lz4.o $(LZ4DIR)/lz4hc.o $(LZ4DIR)/xxhash.o frametest.c
	$(CC) $(FLAGS) $^ -o $@$(EXT)

datagen : $(PRGDIR)/datagen.c datagencli.c
	$(CC) $(FLAGS) -I$(PRGDIR) $^ -o $@$(EXT)

clean:
	@$(MAKE) -C $(LZ4DIR) $@ > $(VOID)
	@$(MAKE) -C $(PRGDIR) $@ > $(VOID)
	@$(RM) core *.o *.test tmp* \
        fullbench-dll$(EXT) fullbench-lib$(EXT) \
        fullbench$(EXT) fullbench32$(EXT) \
        fuzzer$(EXT) fuzzer32$(EXT) \
        frametest$(EXT) frametest32$(EXT) \
        fasttest$(EXT) datagen$(EXT)
	@rm -fR $(TESTDIR)
	@echo Cleaning completed

.PHONY: versionsTest
versionsTest:
	$(PYTHON) test-lz4-versions.py


#-----------------------------------------------------------------------------
# validated only for Linux, OSX, BSD, Hurd and Solaris targets
#-----------------------------------------------------------------------------
ifneq (,$(filter $(shell uname),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS))

MD5:=md5sum
ifneq (,$(filter $(shell uname), Darwin ))
MD5:=md5 -r
endif

DIFF:=diff
ifneq (,$(filter $(shell uname),SunOS))
DIFF:=gdiff
endif


test: test-lz4 test-lz4c test-frametest test-fullbench test-fuzzer

test32: CFLAGS+=-m32
test32: test

test-lz4-sparse: lz4 datagen
	@echo "\n ---- test sparse file support ----"
	./datagen -g5M  -P100 > tmplsdg5M
	$(LZ4) -B4D tmplsdg5M | $(LZ4) -dv --sparse > tmplscB4
	$(DIFF) -s tmplsdg5M tmplscB4
	$(LZ4) -B5D tmplsdg5M | $(LZ4) -dv --sparse > tmplscB5
	$(DIFF) -s tmplsdg5M tmplscB5
	$(LZ4) -B6D tmplsdg5M | $(LZ4) -dv --sparse > tmplscB6
	$(DIFF) -s tmplsdg5M tmplscB6
	$(LZ4) -B7D tmplsdg5M | $(LZ4) -dv --sparse > tmplscB7
	$(DIFF) -s tmplsdg5M tmplscB7
	$(LZ4) tmplsdg5M | $(LZ4) -dv --no-sparse > tmplsnosparse
	$(DIFF) -s tmplsdg5M tmplsnosparse
	ls -ls tmpls*
	./datagen -s1 -g1200007 -P100 | $(LZ4) | $(LZ4) -dv --sparse > tmplsodd   # Odd size file (to generate non-full last block)
	./datagen -s1 -g1200007 -P100 | $(DIFF) -s - tmplsodd
	ls -ls tmplsodd
	@$(RM) tmpls*
	@echo "\n Compatibility with Console :"
	echo "Hello World 1 !" | $(LZ4) | $(LZ4) -d -c
	echo "Hello World 2 !" | $(LZ4) | $(LZ4) -d | cat
	echo "Hello World 3 !" | $(LZ4) --no-frame-crc | $(LZ4) -d -c
	@echo "\n Compatibility with Append :"
	./datagen -P100 -g1M > tmplsdg1M
	cat tmplsdg1M tmplsdg1M > tmpls2M
	$(LZ4) -B5 -v tmplsdg1M tmplsc
	$(LZ4) -d -v tmplsc tmplsr
	$(LZ4) -d -v tmplsc >> tmplsr
	ls -ls tmp*
	$(DIFF) tmpls2M tmplsr
	@$(RM) tmpls*

test-lz4-contentSize: lz4 datagen
	@echo "\n ---- test original size support ----"
	./datagen -g15M > tmplc1
	$(LZ4) -v tmplc1 | $(LZ4) -t
	$(LZ4) -v --content-size tmplc1 | $(LZ4) -d > tmplc2
	$(DIFF) -s tmplc1 tmplc2
	# test large size [2-4] GB
	@./datagen -g3G -P100 | $(LZ4) -vv | $(LZ4) --decompress --force --sparse - tmplc1
	@ls -ls tmplc1
	@./datagen -g3G -P100 | $(LZ4) --quiet --content-size | $(LZ4) --verbose --decompress --force --sparse - tmplc2
	@ls -ls tmplc2
	$(DIFF) -s tmplc1 tmplc2
	@$(RM) tmplc*

test-lz4-frame-concatenation: lz4 datagen
	@echo "\n ---- test frame concatenation ----"
	@echo -n > tmp-lfc-empty
	@echo hi > tmp-lfc-nonempty
	cat tmp-lfc-nonempty tmp-lfc-empty tmp-lfc-nonempty > tmp-lfc-src
	@$(LZ4) -zq tmp-lfc-empty > tmp-lfc-empty.lz4
	@$(LZ4) -zq tmp-lfc-nonempty > tmp-lfc-nonempty.lz4
	cat tmp-lfc-nonempty.lz4 tmp-lfc-empty.lz4 tmp-lfc-nonempty.lz4 > tmp-lfc-concat.lz4
	$(LZ4) -d tmp-lfc-concat.lz4 > tmp-lfc-result
	sdiff tmp-lfc-src tmp-lfc-result
	@$(RM) tmp-lfc-*
	@echo frame concatenation test completed

test-lz4-multiple: lz4 datagen
	@echo "\n ---- test multiple files ----"
	@./datagen -s1        > tmp-tlm1 2> $(VOID)
	@./datagen -s2 -g100K > tmp-tlm2 2> $(VOID)
	@./datagen -s3 -g1M   > tmp-tlm3 2> $(VOID)
	$(LZ4) -f -m tmp-tlm*
	ls -ls tmp-tlm*
	@$(RM) tmp-tlm1 tmp-tlm2 tmp-tlm3
	$(LZ4) -df -m tmp-tlm*.lz4
	ls -ls tmp-tlm*
	$(LZ4) -f -m tmp-tlm1 notHere tmp-tlm2; echo $$?
	@$(RM) tmp-tlm*

test-lz4-basic: lz4 datagen unlz4 lz4cat
	@echo "\n ---- test lz4 basic compression/decompression ----"
	./datagen -g0       | $(LZ4) -v     | $(LZ4) -t
	./datagen -g16KB    | $(LZ4) -9     | $(LZ4) -t
	./datagen -g20KB > tmp-tlb-dg20k
	$(LZ4) < tmp-tlb-dg20k | $(LZ4) -d > tmp-tlb-dec
	$(DIFF) -q tmp-tlb-dg20k tmp-tlb-dec
	$(LZ4) --no-frame-crc < tmp-tlb-dg20k | $(LZ4) -d > tmp-tlb-dec
	$(DIFF) -q tmp-tlb-dg20k tmp-tlb-dec
	./datagen           | $(LZ4)        | $(LZ4) -t
	./datagen -g6M -P99 | $(LZ4) -9BD   | $(LZ4) -t
	./datagen -g17M     | $(LZ4) -9v    | $(LZ4) -qt
	./datagen -g33M     | $(LZ4) --no-frame-crc | $(LZ4) -t
	./datagen -g256MB   | $(LZ4) -vqB4D | $(LZ4) -t
	@echo "hello world" > tmp-tlb-hw
	$(LZ4) --rm -f tmp-tlb-hw tmp-tlb-hw.lz4
	test ! -f tmp-tlb-hw                      # must fail (--rm)
	test   -f tmp-tlb-hw.lz4
	$(PRGDIR)/lz4cat tmp-tlb-hw.lz4           # must display hello world
	test   -f tmp-tlb-hw.lz4
	$(PRGDIR)/unlz4 --rm tmp-tlb-hw.lz4 tmp-tlb-hw
	test   -f tmp-tlb-hw
	test ! -f tmp-tlb-hw.lz4                  # must fail (--rm)
	test ! -f tmp-tlb-hw.lz4.lz4              # must fail (unlz4)
	$(PRGDIR)/lz4cat tmp-tlb-hw               # pass-through mode
	test   -f tmp-tlb-hw
	test ! -f tmp-tlb-hw.lz4                  # must fail (lz4cat)
	$(LZ4) tmp-tlb-hw tmp-tlb-hw.lz4          # creates tmp-tlb-hw.lz4
	$(PRGDIR)/lz4cat < tmp-tlb-hw.lz4 > tmp-tlb3  # checks lz4cat works with stdin (#285)
	$(DIFF) -q tmp-tlb-hw tmp-tlb3
	$(PRGDIR)/lz4cat < tmp-tlb-hw > tmp-tlb2      # checks lz4cat works in pass-through mode
	$(DIFF) -q tmp-tlb-hw tmp-tlb2
	cp tmp-tlb-hw ./-d
	$(LZ4) --rm -- -d -d.lz4               # compresses ./d into ./-d.lz4
	test   -f ./-d.lz4
	test ! -f ./-d
	mv ./-d.lz4 ./-z
	$(LZ4) -d --rm -- -z tmp-tlb4          # uncompresses ./-z into tmp-tlb4
	test ! -f ./-z
	$(DIFF) -q tmp-tlb-hw tmp-tlb4
	$(LZ4) -f tmp-tlb-hw
	cat tmp-tlb-hw >> tmp-tlb-hw.lz4
	$(LZ4) -f tmp-tlb-hw.lz4               # uncompress valid frame followed by invalid data
	$(LZ4) -BX tmp-tlb-hw -c -q | $(LZ4) -tv  # test block checksum
	@$(RM) tmp-tlb*

test-lz4-hugefile: lz4 datagen
	@echo "\n ---- test huge files compression/decompression ----"
	./datagen -g6GB   | $(LZ4) -vB5D  | $(LZ4) -qt
	./datagen -g6GB   | $(LZ4) -v5BD  | $(LZ4) -qt

test-lz4-testmode: lz4 datagen
	@echo "\n ---- bench mode ----"
	$(LZ4) -bi1
	@echo "\n ---- test mode ----"
	! ./datagen | $(LZ4) -t
	! ./datagen | $(LZ4) -tf
	@echo "\n ---- pass-through mode ----"
	! ./datagen | $(LZ4) -d  > $(VOID)
	./datagen | $(LZ4) -df > $(VOID)
	@echo "Hello World !" > tmp-tlt1
	$(LZ4) -dcf tmp-tlt1
	@echo "from underground..." > tmp-tlt2
	$(LZ4) -dcfm tmp-tlt1 tmp-tlt2
	@echo "\n ---- non-existing source ----"
	! $(LZ4)     file-does-not-exist
	! $(LZ4) -f  file-does-not-exist
	! $(LZ4) -fm file1-dne file2-dne
	@$(RM) tmp-tlt

test-lz4-opt-parser: lz4 datagen
	@echo "\n ---- test opt-parser ----"
	./datagen -g16KB      | $(LZ4) -12      | $(LZ4) -t
	./datagen -P10        | $(LZ4) -12B4    | $(LZ4) -t
	./datagen -g256K      | $(LZ4) -12B4D   | $(LZ4) -t
	./datagen -g512K -P25 | $(LZ4) -12BD    | $(LZ4) -t
	./datagen -g1M        | $(LZ4) -12B5    | $(LZ4) -t
	./datagen -g2M -P99   | $(LZ4) -11B4D   | $(LZ4) -t
	./datagen -g4M        | $(LZ4) -11vq    | $(LZ4) -qt
	./datagen -g8M        | $(LZ4) -11B4    | $(LZ4) -t
	./datagen -g16M -P90  | $(LZ4) -11B5    | $(LZ4) -t
	./datagen -g32M -P10  | $(LZ4) -11B5D   | $(LZ4) -t

test-lz4: lz4 datagen test-lz4-basic test-lz4-opt-parser test-lz4-multiple \
          test-lz4-sparse test-lz4-frame-concatenation test-lz4-testmode \
          test-lz4-contentSize test-lz4-hugefile
	@$(RM) tmp*

test-lz4c: lz4c datagen
	@echo "\n ---- test lz4c variant ----"
	./datagen -g256MB | $(LZ4)c -l -v    | $(LZ4)c   -t

test-lz4c32: CFLAGS+=-m32
test-lz4c32: test-lz4

test-interop-32-64: lz4 lz4c32 datagen
	@echo "\n ---- test interoperability 32-bits -vs- 64 bits ----"
	./datagen -g16KB  | $(LZ4)c32 -9     | $(LZ4)    -t
	./datagen -P10    | $(LZ4)    -9B4   | $(LZ4)c32 -t
	./datagen         | $(LZ4)c32        | $(LZ4)    -t
	./datagen -g1M    | $(LZ4)    -3B5   | $(LZ4)c32 -t
	./datagen -g256MB | $(LZ4)c32 -vqB4D | $(LZ4)    -qt
	./datagen -g1G -P90 | $(LZ4)         | $(LZ4)c32 -t
	./datagen -g6GB   | $(LZ4)c32 -vq9BD | $(LZ4)    -qt

test-lz4c32-basic: lz4c32 datagen
	@echo "\n ---- test lz4c32 32-bits version ----"
	./datagen -g16KB  | $(LZ4)c32 -9     | $(LZ4)c32 -t
	./datagen         | $(LZ4)c32        | $(LZ4)c32 -t
	./datagen -g256MB | $(LZ4)c32 -vqB4D | $(LZ4)c32 -qt
	./datagen -g6GB   | $(LZ4)c32 -vqB5D | $(LZ4)c32 -qt

test-platform:
	@echo "\n ---- test lz4 $(QEMU_SYS) platform ----"
	$(QEMU_SYS) ./datagen -g16KB  | $(QEMU_SYS) $(LZ4) -9     | $(QEMU_SYS) $(LZ4) -t
	$(QEMU_SYS) ./datagen         | $(QEMU_SYS) $(LZ4)        | $(QEMU_SYS) $(LZ4) -t
	$(QEMU_SYS) ./datagen -g256MB | $(QEMU_SYS) $(LZ4) -vqB4D | $(QEMU_SYS) $(LZ4) -qt
ifneq ($(QEMU_SYS),qemu-arm-static)
	$(QEMU_SYS) ./datagen -g3GB   | $(QEMU_SYS) $(LZ4) -vqB5D | $(QEMU_SYS) $(LZ4) -qt
endif

test-fullbench: fullbench
	./fullbench --no-prompt $(NB_LOOPS) $(TEST_FILES)

test-fullbench32: CFLAGS += -m32
test-fullbench32: test-fullbench

test-fuzzer: fuzzer
	./fuzzer $(FUZZER_TIME)

test-fuzzer32: CFLAGS += -m32
test-fuzzer32: test-fuzzer

test-frametest: frametest
	./frametest $(FUZZER_TIME)

test-frametest32: CFLAGS += -m32
test-frametest32: test-frametest

test-mem: lz4 datagen fuzzer frametest fullbench
	@echo "\n ---- valgrind tests : memory analyzer ----"
	valgrind --leak-check=yes --error-exitcode=1 ./datagen -g50M > $(VOID)
	./datagen -g16KB > ftmdg16K
	valgrind --leak-check=yes --error-exitcode=1 $(LZ4) -9 -BD -f ftmdg16K $(VOID)
	./datagen -g16KB -s2 > ftmdg16K2
	./datagen -g16KB -s3 > ftmdg16K3
	valgrind --leak-check=yes --error-exitcode=1 $(LZ4) --force --multiple ftmdg16K ftmdg16K2 ftmdg16K3
	./datagen -g16MB > ftmdg16M
	valgrind --leak-check=yes --error-exitcode=1 $(LZ4) -9 -B5D -f ftmdg16M ftmdg16K2
	valgrind --leak-check=yes --error-exitcode=1 $(LZ4) -t ftmdg16K2
	valgrind --leak-check=yes --error-exitcode=1 $(LZ4) -bi1 ftmdg16M
	valgrind --leak-check=yes --error-exitcode=1 ./fullbench -i1 ftmdg16M ftmdg16K2
	./datagen -g256MB > ftmdg256M
	valgrind --leak-check=yes --error-exitcode=1 $(LZ4) -B4D -f -vq ftmdg256M $(VOID)
	$(RM) ftm*
	valgrind --leak-check=yes --error-exitcode=1 ./fuzzer -i64 -t1
	valgrind --leak-check=yes --error-exitcode=1 ./frametest -i256

test-mem32: lz4c32 datagen
# unfortunately, valgrind doesn't seem to work with non-native binary...

endif
