# makefile discription.
# basic build file for mruby

# compiler, linker (gcc)
CC = gcc
LL = gcc
DEBUG_MODE = 1
ifeq ($(DEBUG_MODE),1)
CFLAGS = -g -O3
else
CFLAGS = -O3
endif
ALL_CFLAGS = -Wall -Werror-implicit-function-declaration $(CFLAGS)
ifeq ($(OS),Windows_NT)
  MAKE_FLAGS = --no-print-directory CC=$(CC) LL=$(LL) ALL_CFLAGS='$(ALL_CFLAGS)'
else
  MAKE_FLAGS = --no-print-directory CC='$(CC)' LL='$(LL)' ALL_CFLAGS='$(ALL_CFLAGS)'
endif

##############################
# generic build targets, rules

.PHONY : all
all :
	@$(MAKE) -C src $(MAKE_FLAGS)
	@$(MAKE) -C mrblib $(MAKE_FLAGS)
	@$(MAKE) -C tools/mruby $(MAKE_FLAGS)
	@$(MAKE) -C tools/mirb $(MAKE_FLAGS)

# mruby test
.PHONY : test
test :
	@$(MAKE) -C test $(MAKE_FLAGS)

# clean up
.PHONY : clean
clean :
	@$(MAKE) clean -C src $(MAKE_FLAGS)
	@$(MAKE) clean -C tools/mruby $(MAKE_FLAGS)
	@$(MAKE) clean -C tools/mirb $(MAKE_FLAGS)
	@$(MAKE) clean -C test $(MAKE_FLAGS)
