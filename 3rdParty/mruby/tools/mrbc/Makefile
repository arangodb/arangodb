# makefile discription.
# basic build file for Rite-Compiler
# 11.Apr.2011 coded by Kenji Yoshimoto.
# 31.Aug.2011 coded by Hiroshi Mimaki.

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
BASEDIR := ../../src
TARGET := ../../bin/mrbc
ifeq ($(OS),Windows_NT)
EXE := $(TARGET).exe
else
EXE := $(TARGET)
endif
YSRC := $(BASEDIR)/parse.y
YC := $(BASEDIR)/y.tab.c
EXCEPT1 := $(YC) $(BASEDIR)/minimain.c $(BASEDIR)/load.c $(BASEDIR)/init_ext.c
OBJY := $(patsubst %.c,%.o,$(YC))
OBJ0 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/../tools/mrbc/*.c))
OBJ1 := $(patsubst %.c,%.o,$(filter-out $(EXCEPT1),$(wildcard $(BASEDIR)/*.c)))
#OBJ2 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/regex/*.c))
#OBJ3 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/enc/*.c))
OBJS := $(OBJ0) $(OBJ1) $(OBJ2) $(OBJ3)

# libraries, includes
LIBS = -lm
INCLUDES = -I$(BASEDIR) -I$(BASEDIR)/../include

# compiler, linker (gcc)
CC = gcc
LL = gcc
YACC = bison
DEBUG_MODE = 0
ifeq ($(DEBUG_MODE),1)
CFLAGS = -g
else
CFLAGS = -O2 -g
endif
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)
MAKE_FLAGS = --no-print-directory CC=$(CC) LL=$(LL)

##############################
# generic build targets, rules

.PHONY : all
all : $(EXE)
	@echo "make: built targets of `pwd`"

# executable constructed using linker from object files
$(EXE) : $(OBJS) $(OBJY)
	$(LL) -o $@ $(OBJS) $(OBJY) $(LIBS)

-include $(OBJS:.o=.d) $(OBJY:.o=.d)

# objects compiled from source
$(OBJS) : %.o : %.c
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $< -o $@

# parser complie
$(OBJY) : $(YC)
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $(YC) -o $(OBJY)

# yacc complie
$(YC) : $(YSRC)
	$(YACC) -o $(YC) $(YSRC)

# clean up
.PHONY : clean
clean :
	-rm -f $(EXE) $(OBJS) $(OBJY) $(YC)
	-rm -f $(OBJS:.o=.d) $(OBJY:.o=.d)
	-rm -f $(patsubst %.c,%.o,$(EXCEPT1)) $(patsubst %.c,%.d,$(EXCEPT1))
	@echo "make: removing targets, objects and depend files of `pwd`"

