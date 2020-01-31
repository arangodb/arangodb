# -*- mode: Makefile; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                  COMMON VARIABLES
## -----------------------------------------------------------------------------

SRC=$(shell pwd |sed "s;.*/;;")

.PHONY: warning

warning:
	@echo "ArangoDB has switched to CMAKE. In order to compile, use:"
	@echo ""
	@echo "  mkdir -p build"
	@echo "  cd build"
	@echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
	@echo "  make"
	@echo ""
