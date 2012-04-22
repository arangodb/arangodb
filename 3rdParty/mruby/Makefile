# makefile discription.
# basic build file for Rite-VM(mruby)
# 11.Apr.2011 coded by Kenji Yoshimoto.
# 17.Jan.2012 coded by Hiroshi Mimaki.

# project-specific macros
# extension of the executable-file is modifiable(.exe .out ...)
TARGET := bin/mrubysample
RITEVM := lib/ritevm
MRUBY := tools/mruby/mruby
ifeq ($(OS),Windows_NT)
EXE := $(TARGET).exe
LIB := $(RITEVM).lib
MRB := $(MRUBY).exe
else
EXE := $(TARGET)
LIB := $(RITEVM).a
MRB := $(MRUBY)
endif
MSRC := src/minimain.c
YSRC := src/parse.y
YC := src/y.tab.c
EXCEPT1 := $(YC) $(MSRC)
OBJM := $(patsubst %.c,%.o,$(MSRC))
OBJY := $(patsubst %.c,%.o,$(YC))
OBJ1 := $(patsubst %.c,%.o,$(filter-out $(EXCEPT1),$(wildcard src/*.c)))
#OBJ2 := $(patsubst %.c,%.o,$(wildcard ext/regex/*.c))
#OBJ3 := $(patsubst %.c,%.o,$(wildcard ext/enc/*.c))
OBJS := $(OBJ1) $(OBJ2) $(OBJ3)
# mruby libraries
EXTC := mrblib/mrblib.c
EXTRB := $(wildcard mrblib/*.rb)
EXT0 := $(patsubst %.c,%.o,src/$(EXTC))
# ext libraries
EXTS := $(EXT0)

# libraries, includes
LIBS = $(LIB) -lm
INCLUDES = -I./src -I./include

# library for iOS
IOSLIB := $(RITEVM)-ios.a
IOSSIMLIB := $(RITEVM)-iossim.a
IOSDEVLIB := $(RITEVM)-iosdev.a
IOSSIMCC := xcrun -sdk iphoneos llvm-gcc-4.2 -arch i386 -isysroot "/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator5.0.sdk/"
IOSDEVCC := xcrun -sdk iphoneos llvm-gcc-4.2 -arch armv7 -isysroot "/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk/"

# compiler, linker (gcc)
CC = gcc
LL = gcc
YACC = bison
DEBUG_MODE = 1
ifeq ($(DEBUG_MODE),1)
CFLAGS = -g
else
CFLAGS = -O3
endif
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)
MAKE_FLAGS = --no-print-directory CC=$(CC) LL=$(LL)

##############################
# generic build targets, rules

.PHONY : all
all : $(LIB) $(MRB) $(EXE)
	@echo "make: built targets of `pwd`"

##############################
# make library for iOS
.PHONY : ios
ios : $(IOSLIB)

$(IOSLIB) : $(IOSSIMLIB) $(IOSDEVLIB)
	lipo -arch i386 $(IOSSIMLIB) -arch armv7 $(IOSDEVLIB) -create -output $(IOSLIB)

$(IOSSIMLIB) :
	$(MAKE) clean -C src $(MAKE_FLAGS)
	$(MAKE) -C src $(MAKE_FLAGS) CC="$(IOSSIMCC)" LL="$(IOSSIMCC)"
	cp $(LIB) $(IOSSIMLIB)

$(IOSDEVLIB) :
	$(MAKE) clean -C src $(MAKE_FLAGS)
	$(MAKE) -C src $(MAKE_FLAGS) CC="$(IOSDEVCC)" LL="$(IOSDEVCC)"
	cp $(LIB) $(IOSDEVLIB)

# executable constructed using linker from object files
$(EXE) : $(OBJM) $(LIB)
	$(LL) -o $@ $(OBJM) $(LIBS)

-include $(OBJS:.o=.d)

# src compile
$(LIB) : $(EXTS) $(OBJS) $(OBJY)
	$(MAKE) -C src $(MAKE_FLAGS)

# mruby interpreter compile
$(MRB) : $(EXTS) $(OBJS) $(OBJY)
	$(MAKE) -C tools/mruby $(MAKE_FLAGS)

# objects compiled from source
$(OBJS) :
	$(MAKE) -C src $(MAKE_FLAGS) && $(MAKE) -C tools/mruby $(MAKE_FLAGS)

# extend libraries complile
$(EXTS) : $(EXTRB)
	$(MAKE) -C mrblib $(MAKE_FLAGS)

# test module compile
$(OBJM) : $(MSRC)
	$(CC) $(ALL_CFLAGS) -MMD $(INCLUDES) -c $(MSRC) -o $(OBJM)

# clean up
.PHONY : clean
clean :
	$(MAKE) clean -C src $(MAKE_FLAGS)
	$(MAKE) clean -C tools/mruby $(MAKE_FLAGS)
	-rm -f $(EXE) $(OBJM)
	-rm -f $(OBJM:.o=.d)
	-rm -f $(IOSLIB) $(IOSSIMLIB) $(IOSDEVLIB)
	@echo "make: removing targets, objects and depend files of `pwd`"
