# makefile discription.
# basic build file for mirb executable

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
BASEDIR = ../../src
TARGET := ../../bin/mirb
LIBR := ../../lib/libmruby.a
ifeq ($(OS),Windows_NT)
  EXE := $(TARGET).exe
else
  EXE := $(TARGET)
endif
OBJ0 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/../tools/mirb/*.c))
OBJS := $(OBJ0)

# ext libraries
EXTS := $(EXT1)

# libraries, includes
LIBS = -lm
INCLUDES = -I$(BASEDIR) -I$(BASEDIR)/../include

# compiler, linker (gcc)
CC = gcc
LL = gcc
YACC = bison
DEBUG_MODE = 1
ifeq ($(DEBUG_MODE),1)
  CFLAGS = -g -O3
else
  CFLAGS = -O3
endif
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)
ifeq ($(OS),Windows_NT)
  MAKE_FLAGS = CC=$(CC) LL=$(LL) ALL_CFLAGS="$(ALL_CFLAGS)"
else
  MAKE_FLAGS = CC='$(CC)' LL='$(LL)' ALL_CFLAGS='$(ALL_CFLAGS)'
endif

##############################
# generic build targets, rules

.PHONY : all
all : $(LIBR) $(EXE)

# executable constructed using linker from object files
$(EXE) : $(LIBR) $(OBJS) $(EXTS)
	$(LL) -o $@ $(CFLAGS) $(OBJS) $(LIBR) $(EXTS) $(LIBS)

-include $(OBJS:.o=.d)

# objects compiled from source
$(OBJS) : %.o : %.c
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $< -o $@

# C library compile
$(LIBR) :
	@$(MAKE) -C $(BASEDIR) $(MAKE_FLAGS)

# mruby library compile
# extend libraries complile
$(EXTS) : %.o : %.c
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $< -o $@

# clean up
.PHONY : clean #cleandep
clean :
	$(MAKE) clean -C ../../mrblib $(MAKE_FLAGS)
	$(MAKE) clean -C ../mrbc $(MAKE_FLAGS)
	@echo "make: removing targets, objects and depend files of `pwd`"
	-rm -f $(EXE) $(OBJS)
	-rm -f $(OBJS:.o=.d)
