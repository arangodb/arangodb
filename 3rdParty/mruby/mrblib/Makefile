# makefile discription.
# basic build file for mruby library (Ruby part)

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
BASEDIR = .
TARGET := mrblib
MLIB := $(TARGET).o
CLIB := $(TARGET).c
DLIB := $(TARGET).ctmp
RLIB := $(TARGET).rbtmp
DEPLIB := $(TARGET).d
MRB1 := $(BASEDIR)/*.rb
MRBS := $(MRB1)
LIBR := ../lib/libmruby.a

# C compiler (gcc)
CC = gcc
LL = gcc
AR = ar
DEBUG_MODE = 1
ifeq ($(DEBUG_MODE),1)
CFLAGS = -g
else
CFLAGS = -O3
endif
INCLUDES = -I../src -I../include
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)
ifeq ($(OS),Windows_NT)
  MAKE_FLAGS = CC=$(CC) LL=$(LL) ALL_CFLAGS="$(ALL_CFLAGS)"
else
  MAKE_FLAGS = CC='$(CC)' LL='$(LL)' ALL_CFLAGS='$(ALL_CFLAGS)'
endif

# mruby compiler
ifeq ($(OS),Windows_NT)
MRBC = ../bin/mrbc.exe
else
MRBC = ../bin/mrbc
endif

##############################
# generic build targets, rules

.PHONY : all
all : $(LIBR)

# update libmruby.a
$(LIBR) : $(MRBC) $(MLIB)
	$(AR) r $(LIBR) $(MLIB)

# Compile mrblib source
$(MLIB) : $(CLIB)
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $(CLIB) -o $(MLIB)
	$(AR) r $(LIBR) $(MLIB)

# Compile C source from merged mruby source
$(CLIB) : $(RLIB) $(MRBC)
	$(MRBC) -Bmrblib_irep -o$(DLIB) $(RLIB); cat init_$(TARGET).c $(DLIB) > $@

$(MRBC) : ../src/opcode.h ../src/codegen.c ../src/parse.y
	$(MAKE) -C ../tools/mrbc $(MAKE_FLAGS)

# merge mruby sources
$(RLIB) : $(MRBS)
	cat $? > $@

# clean up
.PHONY : clean
clean :
	@echo "make: removing targets, objects and depend files of `pwd`"
	-rm -f $(MRBC) $(MLIB) $(CLIB) $(RLIB) $(DLIB) $(DEPLIB)

