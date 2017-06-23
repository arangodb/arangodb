# Wrapper makefile to maintain compatibility with the old syntax - don't use.

build-books:
	./build.sh build-books

build-dist-books:
	./build.sh build-dist-books --outputDir "$(OUTPUT_DIR)" --nodeModulesDir "$(NODE_MODULES_DIR)" --cookBook "$(COOKBOOK_DIR)"

clean:
	./build.sh clean

all: build-books

.PHONY: build-books
