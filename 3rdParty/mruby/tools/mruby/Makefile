# makefile discription.
# basic build file for mruby executable

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
BASEDIR = ../../src
TARGET := ../../bin/mruby
LIBR := ../../lib/libmruby.a
ifeq ($(OS),Windows_NT)
EXE := $(TARGET).exe
else
EXE := $(TARGET)
endif
OBJ0 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/../tools/mruby/*.c))
#OBJ1 := $(patsubst %.c,%.o,$(filter-out $(EXCEPT1),$(wildcard $(BASEDIR)/*.c)))
#OBJ2 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/regex/*.c))
#OBJ3 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/enc/*.c))
OBJS := $(OBJ0)

# ext libraries
#EXT1 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/../ext/socket/*.c))
EXTS := $(EXT1)

# libraries, includes
LIBS = -lm
INCLUDES = -I$(BASEDIR) -I$(BASEDIR)/../include

# compiler, linker (gcc)
ifeq ($(strip $(COMPILE_MODE)),)
  # default compile option
  COMPILE_MODE = debug
endif

ifeq ($(COMPILE_MODE),debug)
  CFLAGS = -g -O3
else ifeq ($(COMPILE_MODE),release)
  CFLAGS = -O3
else ifeq ($(COMPILE_MODE),small)
  CFLAGS = -Os
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
	-$(RM_F) $(EXE) $(OBJS)
	-$(RM_F) $(OBJS:.o=.d)
