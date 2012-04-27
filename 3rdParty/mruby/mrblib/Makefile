# makefile discription.
# basic build file for RiteVM library
# 11.Oct.2011 coded by Hiroshi Mimaki.

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
BASEDIR = .
TARGET := mrblib
MLIB := $(TARGET).o
CLIB := $(TARGET).c
DLIB := $(TARGET).ctmp
RLIB := $(TARGET).rbtmp
MRB1 := $(BASEDIR)/*.rb
MRBS := $(MRB1)

# C compiler (gcc)
CC = gcc
LL = gcc
DEBUG_MODE = 1
ifeq ($(DEBUG_MODE),1)
CFLAGS = -g
else
CFLAGS = -O2
endif
INCLUDES = -I../src -I../include
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)
MAKE_FLAGS = --no-print-directory CC=$(CC) LL=$(LL)

# mruby compiler
ifeq ($(OS),Windows_NT)
MRBC = ../bin/mrbc.exe
else
MRBC = ../bin/mrbc
endif

##############################
# generic build targets, rules

.PHONY : all
all : $(MRBC) $(MLIB)
	@echo "make: built targets of `pwd`"

# Compile mrblib source
$(MLIB) : $(CLIB)
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $(CLIB) -o $(MLIB)

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
	-rm -f $(MRBC) $(MLIB) $(CLIB) $(RLIB) $(DLIB)
	-rm -f $(patsubst %.c,%.d,$(CLIB))
	-rm -f $(patsubst %.c,%.o,$(EXCEPT1)) $(patsubst %.c,%.d,$(EXCEPT1))
	@echo "make: removing targets, objects and depend files of `pwd`"

