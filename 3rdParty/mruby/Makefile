# makefile discription.
# basic build file for mruby

# compiler, linker (gcc), archiver, parser generator
export CC = gcc
export LL = gcc
export AR = ar
export YACC = bison

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
  MAKE_FLAGS = --no-print-directory CC=$(CC) LL=$(LL) ALL_CFLAGS='$(ALL_CFLAGS)'
else
  MAKE_FLAGS = --no-print-directory CC='$(CC)' LL='$(LL)' ALL_CFLAGS='$(ALL_CFLAGS)'
endif

##############################
# internal variables

export MSG_BEGIN = @for line in
export MSG_END = ; do echo "$$line"; done

export CP := cp
export RM_F := rm -f
export CAT := cat

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
test : all
	@$(MAKE) -C test $(MAKE_FLAGS)

# clean up
.PHONY : clean
clean :
	@$(MAKE) clean -C src $(MAKE_FLAGS)
	@$(MAKE) clean -C tools/mruby $(MAKE_FLAGS)
	@$(MAKE) clean -C tools/mirb $(MAKE_FLAGS)
	@$(MAKE) clean -C test $(MAKE_FLAGS)

# display help for build configuration and interesting targets
.PHONY : showconfig
showconfig :
	$(MSG_BEGIN) \
	"" \
	"  CC = $(CC)" \
	"  LL = $(LL)" \
	"  MAKE = $(MAKE)" \
	"" \
	"  CFLAGS = $(CFLAGS)" \
	"  ALL_CFLAGS = $(ALL_CFLAGS)" \
	$(MSG_END)

.PHONY : help
help :
	$(MSG_BEGIN) \
	"" \
	"            Basic mruby Makefile" \
	"" \
	"targets:" \
	"  all (default):  build all targets, install (locally) in-repo" \
	"  clean:          clean all built and in-repo installed artifacts" \
	"  showconfig:     show build config summary" \
	"  test:           run all mruby tests" \
	$(MSG_END)
