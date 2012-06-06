# makefile discription.
# basic build file for mruby library

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
BASEDIR = .
TARGET := ../lib/libmruby_core.a
YSRC := $(BASEDIR)/parse.y
YC := $(BASEDIR)/y.tab.c
KWD := $(BASEDIR)/keywords
LDEF := $(BASEDIR)/lex.def
EXCEPT1 := $(YC) $(BASEDIR)/minimain.c
OBJY := $(patsubst %.c,%.o,$(YC))
OBJ1 := $(patsubst %.c,%.o,$(filter-out $(EXCEPT1),$(wildcard $(BASEDIR)/*.c)))
#OBJ2 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/regex/*.c))
#OBJ3 := $(patsubst %.c,%.o,$(wildcard $(BASEDIR)/ext/enc/*.c))
OBJS := $(OBJ1) $(OBJ2) $(OBJ3)

# libraries, includes
INCLUDES = -I$(BASEDIR) -I$(BASEDIR)/../include

DEBUG_MODE = 1
ifeq ($(DEBUG_MODE),1)
CFLAGS = -g
else
CFLAGS = -O3
endif
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)


##############################
# generic build targets, rules

.PHONY : all
all : $(TARGET)

# executable constructed using linker from object files
$(TARGET) : $(OBJS) $(OBJY)
	$(AR) r $@ $(OBJS) $(OBJY)

-include $(OBJS:.o=.d) $(OBJY:.o=.d)

# objects compiled from source
$(OBJS) : %.o : %.c
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $< -o $@

# parser complie
$(OBJY) : $(YC) $(LDEF)
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $(YC) -o $(OBJY)

# yacc complie
$(YC) : $(YSRC)
	$(YACC) -o $(YC) $(YSRC)

$(LDEF) : $(KWD)
	gperf -L ANSI-C -C -p -j1 -i 1 -g -o -t -N mrb_reserved_word -k'1,3,$$' $(KWD) > $(LDEF)

# clean up
.PHONY : clean #cleandep
clean :
	@echo "make: removing targets, objects and depend files of `pwd`"
	-$(RM_F) $(TARGET) $(OBJS) $(OBJY) $(YC)
	-$(RM_F) $(OBJS:.o=.d) $(OBJY:.o=.d)
	-$(RM_F) $(patsubst %.c,%.o,$(EXCEPT1)) $(patsubst %.c,%.d,$(EXCEPT1))
