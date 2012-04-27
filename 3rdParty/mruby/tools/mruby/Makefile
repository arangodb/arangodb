# makefile discription.
# basic build file for Rite-Interpreter
# 11.Apr.2011 coded by Kenji Yoshimoto.
# 31.Aug.2011 coded by Hiroshi Mimaki.

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
BASEDIR = ../../src
TARGET := ../../bin/mruby
ifeq ($(OS),Windows_NT)
EXE := $(TARGET).exe
else
EXE := $(TARGET)
endif
YSRC := $(BASEDIR)/parse.y
YC := $(BASEDIR)/y.tab.c
EXCEPT1 := $(YC) $(BASEDIR)/minimain.c $(BASEDIR)/dump.c $(BASEDIR)/cdump.c
OBJY := $(patsubst %.c,%.o,$(YC))
OBJ0 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/../tools/mruby/*.c))
OBJ1 := $(patsubst %.c,%.o,$(filter-out $(EXCEPT1),$(wildcard $(BASEDIR)/*.c)))
#OBJ2 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/regex/*.c))
#OBJ3 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/enc/*.c))
OBJS := $(OBJ0) $(OBJ1) $(OBJ2) $(OBJ3)
# mruby libraries
EXTC := $(BASEDIR)/../mrblib/mrblib.c
EXTRB := $(wildcard $(BASEDIR)/../mrblib/*.rb)
EXTM := $(patsubst %.c,%.o,$(EXTC))
# ext libraries
#EXT1 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/../ext/socket/*.c))
EXTS := $(EXT1)

# libraries, includes
LIBS = -lm
INCLUDES = -I$(BASEDIR) -I$(BASEDIR)/../include
#INCLUDES = -I$(RITEVM_ROOT)

# compiler, linker (gcc)
CC = gcc
LL = gcc
YACC = bison
DEBUG_MODE = 1
ifeq ($(DEBUG_MODE),1)
CFLAGS = -g
else
CFLAGS = -O2
endif
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)
MAKE_FLAGS = --no-print-directory CC=$(CC) LL=$(LL)

##############################
# generic build targets, rules

.PHONY : all
all : $(EXTM) $(EXE)
	@echo "make: built targets of `pwd`"

# executable constructed using linker from object files
$(EXE) : $(OBJS) $(OBJY) $(EXTM) $(EXTS)
	$(LL) -o $@ $(OBJS) $(OBJY) $(EXTM) $(EXTS) $(LIBS)

-include $(OBJS:.o=.d) $(OBJY:.o=.d)

# objects compiled from source
$(OBJS) : %.o : %.c
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $< -o $@

# mruby library compile
$(EXTM) : $(EXTRB) $(OBJS) $(OBJY)
	$(MAKE) -C ../../mrblib $(MAKE_FLAGS)

# extend libraries complile
$(EXTS) : %.o : %.c
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $< -o $@

# parser complie
$(OBJY) : $(YC)
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $(YC) -o $(OBJY)

# yacc complie
$(YC) : $(YSRC)
	echo $(YACC) -o $(YC) $(YSRC)
	touch $(YC)

# clean up
.PHONY : clean #cleandep
clean :
	$(MAKE) clean -C ../../mrblib $(MAKE_FLAGS)
	-rm -f $(EXE) $(OBJS) $(OBJY) $(YC) $(EXTS)
	-rm -f $(OBJS:.o=.d) $(OBJY:.o=.d) $(EXTS:.o=.d)
	-rm -f $(patsubst %.c,%.o,$(EXCEPT1)) $(patsubst %.c,%.d,$(EXCEPT1))
	@echo "make: removing targets, objects and depend files of `pwd`"
