# -*- makefile -*-

# After changing this, run `make update_version` to update various sources
# which hard-code it.
SNOWBALL_VERSION = 2.0.0

c_src_dir = src_c

JAVAC ?= javac
JAVA ?= java
java_src_main_dir = java/org/tartarus/snowball
java_src_dir = $(java_src_main_dir)/ext

MONO ?= mono
MCS ?= mcs
csharp_src_main_dir = csharp/Snowball
csharp_src_dir = $(csharp_src_main_dir)/Algorithms
csharp_sample_dir = csharp/Stemwords

FPC ?= fpc
pascal_src_dir = pascal

python ?= python3
python_output_dir = python_out
python_runtime_dir = snowballstemmer
python_sample_dir = sample

js_output_dir = js_out
js_runtime_dir = javascript
js_sample_dir = sample
NODE ?= nodejs

cargo ?= cargo
cargoflags ?=
rust_src_main_dir = rust/src
rust_src_dir = $(rust_src_main_dir)/snowball/algorithms

go ?= go
goflags ?= stemwords/algorithms.go stemwords/main.go
gofmt ?= gofmt
go_src_main_dir = go
go_src_dir = $(go_src_main_dir)/algorithms

ICONV = iconv
#ICONV = python ./iconv.py

# algorithms.mk is generated from libstemmer/modules.txt and defines:
# * libstemmer_algorithms
# * ISO_8859_1_algorithms
# * ISO_8859_2_algorithms
# * KOI8_R_algorithms
include algorithms.mk $(shell libstemmer/mkalgorithms.pl algorithms.mk libstemmer/modules.txt >/dev/null)

other_algorithms = german2 kraaij_pohlmann lovins

all_algorithms = $(libstemmer_algorithms) $(other_algorithms)

COMPILER_SOURCES = compiler/space.c \
		   compiler/tokeniser.c \
		   compiler/analyser.c \
		   compiler/generator.c \
		   compiler/driver.c \
		   compiler/generator_csharp.c \
		   compiler/generator_java.c \
		   compiler/generator_js.c \
		   compiler/generator_pascal.c \
		   compiler/generator_python.c \
		   compiler/generator_rust.c \
		   compiler/generator_go.c

COMPILER_HEADERS = compiler/header.h \
		   compiler/syswords.h \
		   compiler/syswords2.h

RUNTIME_SOURCES  = runtime/api.c \
		   runtime/utilities.c

RUNTIME_HEADERS  = runtime/api.h \
		   runtime/header.h

JAVARUNTIME_SOURCES = java/org/tartarus/snowball/Among.java \
		      java/org/tartarus/snowball/SnowballProgram.java \
		      java/org/tartarus/snowball/SnowballStemmer.java \
		      java/org/tartarus/snowball/TestApp.java

CSHARP_RUNTIME_SOURCES = csharp/Snowball/Among.cs \
			 csharp/Snowball/Stemmer.cs \
			 csharp/Snowball/AssemblyInfo.cs

CSHARP_STEMWORDS_SOURCES = csharp/Stemwords/Program.cs

JS_RUNTIME_SOURCES = javascript/base-stemmer.js

JS_SAMPLE_SOURCES = javascript/stemwords.js

PASCAL_RUNTIME_SOURCES = pascal/SnowballProgram.pas

PASCAL_STEMWORDS_SOURCES = pascal/stemwords.dpr

PYTHON_RUNTIME_SOURCES = python/snowballstemmer/basestemmer.py \
		         python/snowballstemmer/among.py

PYTHON_SAMPLE_SOURCES = python/testapp.py \
		        python/stemwords.py

PYTHON_PACKAGE_FILES = python/MANIFEST.in \
		       python/setup.py \
		       python/setup.cfg

LIBSTEMMER_SOURCES = libstemmer/libstemmer.c
LIBSTEMMER_UTF8_SOURCES = libstemmer/libstemmer_utf8.c
LIBSTEMMER_HEADERS = include/libstemmer.h libstemmer/modules.h libstemmer/modules_utf8.h
LIBSTEMMER_EXTRA = libstemmer/modules.txt libstemmer/libstemmer_c.in

STEMWORDS_SOURCES = examples/stemwords.c

PYTHON_STEMWORDS_SOURCE = python/stemwords.py

COMMON_FILES = COPYING \
	       NEWS

ALL_ALGORITHM_FILES = $(all_algorithms:%=algorithms/%.sbl)
C_LIB_SOURCES = $(libstemmer_algorithms:%=$(c_src_dir)/stem_UTF_8_%.c) \
		$(KOI8_R_algorithms:%=$(c_src_dir)/stem_KOI8_R_%.c) \
		$(ISO_8859_1_algorithms:%=$(c_src_dir)/stem_ISO_8859_1_%.c) \
		$(ISO_8859_2_algorithms:%=$(c_src_dir)/stem_ISO_8859_2_%.c)
C_LIB_HEADERS = $(libstemmer_algorithms:%=$(c_src_dir)/stem_UTF_8_%.h) \
		$(KOI8_R_algorithms:%=$(c_src_dir)/stem_KOI8_R_%.h) \
		$(ISO_8859_1_algorithms:%=$(c_src_dir)/stem_ISO_8859_1_%.h) \
		$(ISO_8859_2_algorithms:%=$(c_src_dir)/stem_ISO_8859_2_%.h)
C_OTHER_SOURCES = $(other_algorithms:%=$(c_src_dir)/stem_UTF_8_%.c)
C_OTHER_HEADERS = $(other_algorithms:%=$(c_src_dir)/stem_UTF_8_%.h)
JAVA_SOURCES = $(libstemmer_algorithms:%=$(java_src_dir)/%Stemmer.java)
CSHARP_SOURCES = $(libstemmer_algorithms:%=$(csharp_src_dir)/%Stemmer.generated.cs)
PASCAL_SOURCES = $(ISO_8859_1_algorithms:%=$(pascal_src_dir)/%Stemmer.pas)
PYTHON_SOURCES = $(libstemmer_algorithms:%=$(python_output_dir)/%_stemmer.py) \
		 $(python_output_dir)/__init__.py
JS_SOURCES = $(libstemmer_algorithms:%=$(js_output_dir)/%-stemmer.js)
RUST_SOURCES = $(libstemmer_algorithms:%=$(rust_src_dir)/%_stemmer.rs)
GO_SOURCES = $(libstemmer_algorithms:%=$(go_src_dir)/%_stemmer.go) \
	$(go_src_main_dir)/stemwords/algorithms.go

COMPILER_OBJECTS=$(COMPILER_SOURCES:.c=.o)
RUNTIME_OBJECTS=$(RUNTIME_SOURCES:.c=.o)
LIBSTEMMER_OBJECTS=$(LIBSTEMMER_SOURCES:.c=.o)
LIBSTEMMER_UTF8_OBJECTS=$(LIBSTEMMER_UTF8_SOURCES:.c=.o)
STEMWORDS_OBJECTS=$(STEMWORDS_SOURCES:.c=.o)
C_LIB_OBJECTS = $(C_LIB_SOURCES:.c=.o)
C_OTHER_OBJECTS = $(C_OTHER_SOURCES:.c=.o)
JAVA_CLASSES = $(JAVA_SOURCES:.java=.class)
JAVA_RUNTIME_CLASSES=$(JAVARUNTIME_SOURCES:.java=.class)

CFLAGS=-O2 -W -Wall -Wmissing-prototypes -Wmissing-declarations
CPPFLAGS=-Iinclude

all: snowball libstemmer.o stemwords $(C_OTHER_SOURCES) $(C_OTHER_HEADERS) $(C_OTHER_OBJECTS)

clean:
	rm -f $(COMPILER_OBJECTS) $(RUNTIME_OBJECTS) \
	      $(LIBSTEMMER_OBJECTS) $(LIBSTEMMER_UTF8_OBJECTS) $(STEMWORDS_OBJECTS) snowball \
	      libstemmer.o stemwords \
              libstemmer/modules.h \
              libstemmer/modules_utf8.h \
	      $(C_LIB_SOURCES) $(C_LIB_HEADERS) $(C_LIB_OBJECTS) \
	      $(C_OTHER_SOURCES) $(C_OTHER_HEADERS) $(C_OTHER_OBJECTS) \
	      $(JAVA_SOURCES) $(JAVA_CLASSES) $(JAVA_RUNTIME_CLASSES) \
	      $(CSHARP_SOURCES) \
	      $(PASCAL_SOURCES) pascal/stemwords.dpr pascal/stemwords pascal/*.o pascal/*.ppu \
	      $(PYTHON_SOURCES) \
	      $(JS_SOURCES) \
	      $(RUST_SOURCES) \
              libstemmer/mkinc.mak libstemmer/mkinc_utf8.mak \
              libstemmer/libstemmer.c libstemmer/libstemmer_utf8.c \
	      algorithms.mk
	rm -rf dist
	-rmdir $(c_src_dir)
	-rmdir $(python_output_dir)
	-rmdir $(js_output_dir)

snowball: $(COMPILER_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(COMPILER_OBJECTS): $(COMPILER_HEADERS)

libstemmer/libstemmer.c: libstemmer/libstemmer_c.in
	sed 's/@MODULES_H@/modules.h/' $^ >$@

libstemmer/libstemmer_utf8.c: libstemmer/libstemmer_c.in
	sed 's/@MODULES_H@/modules_utf8.h/' $^ >$@

libstemmer/modules.h libstemmer/mkinc.mak: libstemmer/mkmodules.pl libstemmer/modules.txt
	libstemmer/mkmodules.pl $@ $(c_src_dir) libstemmer/modules.txt libstemmer/mkinc.mak

libstemmer/modules_utf8.h libstemmer/mkinc_utf8.mak: libstemmer/mkmodules.pl libstemmer/modules.txt
	libstemmer/mkmodules.pl $@ $(c_src_dir) libstemmer/modules.txt libstemmer/mkinc_utf8.mak utf8

libstemmer/libstemmer.o: libstemmer/modules.h $(C_LIB_HEADERS)

libstemmer.o: libstemmer/libstemmer.o $(RUNTIME_OBJECTS) $(C_LIB_OBJECTS)
	$(AR) -cru $@ $^

stemwords: $(STEMWORDS_OBJECTS) libstemmer.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

csharp_stemwords: $(CSHARP_STEMWORDS_SOURCES) $(CSHARP_RUNTIME_SOURCES) $(CSHARP_SOURCES)
	$(MCS) -unsafe -target:exe -out:$@ $(CSHARP_STEMWORDS_SOURCES) $(CSHARP_RUNTIME_SOURCES) $(CSHARP_SOURCES)

pascal/stemwords.dpr: pascal/stemwords-template.dpr libstemmer/modules.txt
	pascal/generate.pl $(ISO_8859_1_algorithms) < pascal/stemwords-template.dpr > $@

pascal/stemwords: $(PASCAL_STEMWORDS_SOURCES) $(PASCAL_RUNTIME_SOURCES) $(PASCAL_SOURCES)
	$(FPC) -o$@ -Mdelphi $(PASCAL_STEMWORDS_SOURCES)

$(c_src_dir)/stem_UTF_8_%.c $(c_src_dir)/stem_UTF_8_%.h: algorithms/%.sbl snowball
	@mkdir -p $(c_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(c_src_dir)/stem_UTF_8_$${l}"; \
	echo "./snowball $< -o $${o} -eprefix $${l}_UTF_8_ -r ../runtime -u"; \
	./snowball $< -o $${o} -eprefix $${l}_UTF_8_ -r ../runtime -u

$(c_src_dir)/stem_KOI8_R_%.c $(c_src_dir)/stem_KOI8_R_%.h: algorithms/%.sbl snowball
	@mkdir -p $(c_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(c_src_dir)/stem_KOI8_R_$${l}"; \
	echo "./snowball charsets/KOI8-R.sbl $< -o $${o} -eprefix $${l}_KOI8_R_ -r ../runtime"; \
	./snowball charsets/KOI8-R.sbl $< -o $${o} -eprefix $${l}_KOI8_R_ -r ../runtime

$(c_src_dir)/stem_ISO_8859_1_%.c $(c_src_dir)/stem_ISO_8859_1_%.h: algorithms/%.sbl snowball
	@mkdir -p $(c_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(c_src_dir)/stem_ISO_8859_1_$${l}"; \
	echo "./snowball $< -o $${o} -eprefix $${l}_ISO_8859_1_ -r ../runtime"; \
	./snowball $< -o $${o} -eprefix $${l}_ISO_8859_1_ -r ../runtime

$(c_src_dir)/stem_ISO_8859_2_%.c $(c_src_dir)/stem_ISO_8859_2_%.h: algorithms/%.sbl snowball
	@mkdir -p $(c_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(c_src_dir)/stem_ISO_8859_2_$${l}"; \
	echo "./snowball charsets/ISO-8859-2.sbl $< -o $${o} -eprefix $${l}_ISO_8859_2_ -r ../runtime"; \
	./snowball charsets/ISO-8859-2.sbl $< -o $${o} -eprefix $${l}_ISO_8859_2_ -r ../runtime

$(c_src_dir)/stem_%.o: $(c_src_dir)/stem_%.c $(c_src_dir)/stem_%.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(java_src_dir)/%Stemmer.java: algorithms/%.sbl snowball
	@mkdir -p $(java_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(java_src_dir)/$${l}Stemmer"; \
	echo "./snowball $< -j -o $${o} -p org.tartarus.snowball.SnowballStemmer"; \
	./snowball $< -j -o $${o} -p org.tartarus.snowball.SnowballStemmer

$(csharp_src_dir)/%Stemmer.generated.cs: algorithms/%.sbl snowball
	@mkdir -p $(csharp_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	t=`echo "$${l}" | sed 's/.*/\L&/; s/[a-z]*/\u&/g'`; \
	o="$(csharp_src_dir)/$${l}Stemmer.generated"; \
	echo "./snowball $< -cs -o $${o}"; \
	./snowball $< -cs -o $${o}

$(pascal_src_dir)/%Stemmer.pas: algorithms/%.sbl snowball
	@mkdir -p $(pascal_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	t=`echo "$${l}" | sed 's/.*/\L&/; s/[a-z]*/\u&/g'`; \
	o="$(pascal_src_dir)/$${l}Stemmer"; \
	echo "./snowball $< -pascal -o $${o}"; \
	./snowball $< -pascal -o $${o}

$(python_output_dir)/%_stemmer.py: algorithms/%.sbl snowball
	@mkdir -p $(python_output_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(python_output_dir)/$${l}_stemmer"; \
	echo "./snowball $< -py -o $${o}"; \
	./snowball $< -py -o $${o}

$(python_output_dir)/__init__.py: libstemmer/modules.txt
	@mkdir -p $(python_output_dir)
	$(python) python/create_init.py $(python_output_dir)

$(rust_src_dir)/%_stemmer.rs: algorithms/%.sbl snowball
	@mkdir -p $(rust_src_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(rust_src_dir)/$${l}_stemmer"; \
	echo "./snowball $< -rust -o $${o}"; \
	./snowball $< -rust -o $${o}

$(go_src_main_dir)/stemwords/algorithms.go:
	@echo "Generating algorithms.go"
	@cd go/stemwords && go generate

$(go_src_dir)/%_stemmer.go: algorithms/%.sbl snowball
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(go_src_dir)/$${l}/$${l}_stemmer"; \
	mkdir -p $(go_src_dir)/$${l}
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(go_src_dir)/$${l}/$${l}_stemmer"; \
	echo "./snowball $< -go -o $${o} -gop $${l}"; \
	./snowball $< -go -o $${o} -gop $${l}
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(go_src_dir)/$${l}/$${l}_stemmer"; \
	echo "$(gofmt) -s -w $(go_src_dir)/$${l}/$${l}_stemmer.go"; \
	$(gofmt) -s -w $(go_src_dir)/$${l}/$${l}_stemmer.go

$(js_output_dir)/%-stemmer.js: algorithms/%.sbl snowball
	@mkdir -p $(js_output_dir)
	@l=`echo "$<" | sed 's!\(.*\)\.sbl$$!\1!;s!^.*/!!'`; \
	o="$(js_output_dir)/$${l}-stemmer"; \
	echo "./snowball $< -js -o $${o}"; \
	./snowball $< -js -o $${o}

# Make a full source distribution
dist: dist_snowball dist_libstemmer_c dist_libstemmer_csharp dist_libstemmer_java dist_libstemmer_js dist_libstemmer_python

# Make a distribution of all the sources involved in snowball
dist_snowball: $(COMPILER_SOURCES) $(COMPILER_HEADERS) \
	    $(RUNTIME_SOURCES) $(RUNTIME_HEADERS) \
	    $(LIBSTEMMER_SOURCES) \
	    $(LIBSTEMMER_UTF8_SOURCES) \
            $(LIBSTEMMER_HEADERS) \
	    $(LIBSTEMMER_EXTRA) \
	    $(ALL_ALGORITHM_FILES) $(STEMWORDS_SOURCES) \
	    $(COMMON_FILES) \
	    GNUmakefile README doc/TODO libstemmer/mkmodules.pl
	destname=snowball_code; \
	dest=dist/$${destname}; \
	rm -rf $${dest} && \
	rm -f $${dest}.tgz && \
	for file in $^; do \
	  dir=`dirname $$file` && \
	  mkdir -p $${dest}/$${dir} && \
	  cp -a $${file} $${dest}/$${dir} || exit 1 ; \
	done && \
	(cd dist && tar zcf $${destname}.tgz $${destname}) && \
	rm -rf $${dest}

# Make a distribution of all the sources required to compile the C library.
dist_libstemmer_c: \
            $(RUNTIME_SOURCES) \
            $(RUNTIME_HEADERS) \
            $(LIBSTEMMER_SOURCES) \
            $(LIBSTEMMER_UTF8_SOURCES) \
            $(LIBSTEMMER_HEADERS) \
            $(LIBSTEMMER_EXTRA) \
	    $(C_LIB_SOURCES) \
            $(C_LIB_HEADERS) \
            libstemmer/mkinc.mak \
            libstemmer/mkinc_utf8.mak
	destname=libstemmer_c; \
	dest=dist/$${destname}; \
	rm -rf $${dest} && \
	rm -f $${dest}.tgz && \
	mkdir -p $${dest} && \
	cp -a doc/libstemmer_c_README $${dest}/README && \
	mkdir -p $${dest}/examples && \
	cp -a examples/stemwords.c $${dest}/examples && \
	mkdir -p $${dest}/$(c_src_dir) && \
	cp -a $(C_LIB_SOURCES) $(C_LIB_HEADERS) $${dest}/$(c_src_dir) && \
	mkdir -p $${dest}/runtime && \
	cp -a $(RUNTIME_SOURCES) $(RUNTIME_HEADERS) $${dest}/runtime && \
	mkdir -p $${dest}/libstemmer && \
	cp -a $(LIBSTEMMER_SOURCES) $(LIBSTEMMER_UTF8_SOURCES) $(LIBSTEMMER_HEADERS) $(LIBSTEMMER_EXTRA) $${dest}/libstemmer && \
	mkdir -p $${dest}/include && \
	mv $${dest}/libstemmer/libstemmer.h $${dest}/include && \
	(cd $${dest} && \
	 echo "README" >> MANIFEST && \
	 ls $(c_src_dir)/*.c $(c_src_dir)/*.h >> MANIFEST && \
	 ls runtime/*.c runtime/*.h >> MANIFEST && \
	 ls libstemmer/*.c libstemmer/*.h >> MANIFEST && \
	 ls include/*.h >> MANIFEST) && \
        cp -a libstemmer/mkinc.mak libstemmer/mkinc_utf8.mak $${dest}/ && \
	cp -a $(COMMON_FILES) $${dest} && \
	echo 'include mkinc.mak' >> $${dest}/Makefile && \
	echo 'CFLAGS=-O2' >> $${dest}/Makefile && \
	echo 'CPPFLAGS=-Iinclude' >> $${dest}/Makefile && \
	echo 'all: libstemmer.o stemwords' >> $${dest}/Makefile && \
	echo 'libstemmer.o: $$(snowball_sources:.c=.o)' >> $${dest}/Makefile && \
	echo '	$$(AR) -cru $$@ $$^' >> $${dest}/Makefile && \
	echo 'stemwords: examples/stemwords.o libstemmer.o' >> $${dest}/Makefile && \
	echo '	$$(CC) $$(CFLAGS) -o $$@ $$^' >> $${dest}/Makefile && \
	echo 'clean:' >> $${dest}/Makefile && \
	echo '	rm -f stemwords *.o $(c_src_dir)/*.o examples/*.o runtime/*.o libstemmer/*.o' >> $${dest}/Makefile && \
	(cd dist && tar zcf $${destname}.tgz $${destname}) && \
	rm -rf $${dest}

# Make a distribution of all the sources required to compile the Java library.
dist_libstemmer_java: $(RUNTIME_SOURCES) $(RUNTIME_HEADERS) \
            $(LIBSTEMMER_EXTRA) \
	    $(JAVA_SOURCES)
	destname=libstemmer_java; \
	dest=dist/$${destname}; \
	rm -rf $${dest} && \
	rm -f $${dest}.tgz && \
	mkdir -p $${dest} && \
	cp -a doc/libstemmer_java_README $${dest}/README && \
	mkdir -p $${dest}/$(java_src_dir) && \
	cp -a $(JAVA_SOURCES) $${dest}/$(java_src_dir) && \
	mkdir -p $${dest}/$(java_src_main_dir) && \
	cp -a $(JAVARUNTIME_SOURCES) $${dest}/$(java_src_main_dir) && \
	cp -a $(COMMON_FILES) $${dest} && \
	(cd $${dest} && \
	 echo "README" >> MANIFEST && \
	 ls $(java_src_dir)/*.java >> MANIFEST && \
	 ls $(java_src_main_dir)/*.java >> MANIFEST) && \
	(cd dist && tar zcf $${destname}.tgz $${destname}) && \
	rm -rf $${dest}

# Make a distribution of all the sources required to compile the C# library.
dist_libstemmer_csharp: $(RUNTIME_SOURCES) $(RUNTIME_HEADERS) \
            $(LIBSTEMMER_EXTRA) \
	    $(CSHARP_SOURCES)
	destname=libstemmer_csharp; \
	dest=dist/$${destname}; \
	rm -rf $${dest} && \
	rm -f $${dest}.tgz && \
	mkdir -p $${dest} && \
	cp -a doc/libstemmer_csharp_README $${dest}/README && \
	mkdir -p $${dest}/$(csharp_src_dir) && \
	cp -a $(CSHARP_SOURCES) $${dest}/$(csharp_src_dir) && \
	mkdir -p $${dest}/$(csharp_src_main_dir) && \
	cp -a $(CSHARP_RUNTIME_SOURCES) $${dest}/$(csharp_src_main_dir) && \
	mkdir -p $${dest}/$(csharp_sample_dir) && \
	cp -a $(CSHARP_STEMWORDS_SOURCES) $${dest}/$(csharp_sample_dir) && \
	cp -a $(COMMON_FILES) $${dest} && \
	(cd dist && tar zcf $${destname}.tgz $${destname}) && \
	rm -rf $${dest}

dist_libstemmer_python: $(PYTHON_SOURCES)
	destname=snowballstemmer; \
	dest=dist/$${destname}; \
	rm -rf $${dest} && \
	rm -f $${dest}.tgz && \
	mkdir -p $${dest} && \
	mkdir -p $${dest}/src/$(python_runtime_dir) && \
	mkdir -p $${dest}/src/$(python_sample_dir) && \
	cp libstemmer/modules.txt $${dest} && \
	cp doc/libstemmer_python_README $${dest}/README.rst && \
	cp -a $(PYTHON_SOURCES) $${dest}/src/$(python_runtime_dir) && \
	cp -a $(PYTHON_SAMPLE_SOURCES) $${dest}/src/$(python_sample_dir) && \
	cp -a $(PYTHON_RUNTIME_SOURCES) $${dest}/src/$(python_runtime_dir) && \
	cp -a $(COMMON_FILES) $(PYTHON_PACKAGE_FILES) $${dest} && \
	(cd $${dest} && $(python) setup.py sdist bdist_wheel && cp dist/*.tar.gz dist/*.whl ..) && \
	rm -rf $${dest}

dist_libstemmer_js: $(JS_SOURCES)
	destname=jsstemmer; \
	dest=dist/$${destname}; \
	rm -rf $${dest} && \
	rm -f $${dest}.tgz && \
	mkdir -p $${dest} && \
	mkdir -p $${dest}/$(js_runtime_dir) && \
	mkdir -p $${dest}/$(js_sample_dir) && \
	cp -a doc/libstemmer_js_README $${dest}/README && \
	cp -a $(COMMON_FILES) $${dest} && \
	cp -a $(JS_RUNTIME_SOURCES) $${dest}/$(js_runtime_dir) && \
	cp -a $(JS_SAMPLE_SOURCES) $${dest}/$(js_sample_dir) && \
	cp -a $(JS_SOURCES) $${dest}/$(js_runtime_dir) && \
	(cd $${dest} && \
	 ls README $(COMMON_FILES) $(js_runtime_dir)/*.js $(js_sample_dir)/*.js > MANIFEST) && \
	(cd dist && tar zcf $${destname}.tgz $${destname}) && \
	rm -rf $${dest}

check: check_utf8 check_iso_8859_1 check_iso_8859_2 check_koi8r

check_utf8: $(libstemmer_algorithms:%=check_utf8_%)

check_iso_8859_1: $(ISO_8859_1_algorithms:%=check_iso_8859_1_%)

check_iso_8859_2: $(ISO_8859_2_algorithms:%=check_iso_8859_2_%)

check_koi8r: $(KOI8_R_algorithms:%=check_koi8r_%)

# Where the data files are located - assumes their repo is checked out as
# a sibling to this one.
STEMMING_DATA ?= ../snowball-data
STEMMING_DATA_ABS := $(abspath $(STEMMING_DATA))

check_utf8_%: $(STEMMING_DATA)/% stemwords
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer with UTF-8"
	@if test -f '$</voc.txt.gz' ; then \
	  gzip -dc '$</voc.txt.gz'|./stemwords -c UTF_8 -l `echo $<|sed 's!.*/!!'` -o tmp.txt; \
	else \
	  ./stemwords -c UTF_8 -l `echo $<|sed 's!.*/!!'` -i $</voc.txt -o tmp.txt; \
	fi
	@if test -f '$</output.txt.gz' ; then \
	  gzip -dc '$</output.txt.gz'|diff -u - tmp.txt; \
	else \
	  diff -u $</output.txt tmp.txt; \
	fi
	@rm tmp.txt

check_iso_8859_1_%: $(STEMMING_DATA)/% stemwords
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer with ISO_8859_1"
	@$(ICONV) -fUTF8 -tISO8859-1 '$</voc.txt' |\
	    ./stemwords -c ISO_8859_1 -l `echo $<|sed 's!.*/!!'` -o tmp.txt
	@$(ICONV) -fUTF8 -tISO8859-1 '$</output.txt' |\
	    diff -u - tmp.txt
	@rm tmp.txt

check_iso_8859_2_%: $(STEMMING_DATA)/% stemwords
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer with ISO_8859_2"
	@$(ICONV) -fUTF8 -tISO8859-2 '$</voc.txt' |\
	    ./stemwords -c ISO_8859_2 -l `echo $<|sed 's!.*/!!'` -o tmp.txt
	@$(ICONV) -fUTF8 -tISO8859-2 '$</output.txt' |\
	    diff -u - tmp.txt
	@rm tmp.txt

check_koi8r_%: $(STEMMING_DATA)/% stemwords
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer with KOI8R"
	@$(ICONV) -fUTF8 -tKOI8-R '$</voc.txt' |\
	    ./stemwords -c KOI8_R -l `echo $<|sed 's!.*/!!'` -o tmp.txt
	@$(ICONV) -fUTF8 -tKOI8-R '$</output.txt' |\
	    diff -u - tmp.txt
	@rm tmp.txt

.java.class:
	cd java && $(JAVAC) `echo "$<"|sed 's,^java/,,'`

check_java: $(JAVA_CLASSES) $(JAVA_RUNTIME_CLASSES)
	$(MAKE) do_check_java

do_check_java: $(libstemmer_algorithms:%=check_java_%)

check_java_%: $(STEMMING_DATA_ABS)/%
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer for Java"
	@cd java && if test -f '$</voc.txt.gz' ; then \
	  gzip -dc '$</voc.txt.gz' |\
	    $(JAVA) org/tartarus/snowball/TestApp `echo $<|sed 's!.*/!!'` -o $(PWD)/tmp.txt; \
	else \
	  $(JAVA) org/tartarus/snowball/TestApp `echo $<|sed 's!.*/!!'` $</voc.txt -o $(PWD)/tmp.txt; \
	fi
	@if test -f '$</output.txt.gz' ; then \
	  gzip -dc '$</output.txt.gz'|diff -u - tmp.txt; \
	else \
	  diff -u $</output.txt tmp.txt; \
	fi
	@rm tmp.txt

check_csharp: csharp_stemwords
	$(MAKE) do_check_csharp

do_check_csharp: $(libstemmer_algorithms:%=check_csharp_%)

check_csharp_%: $(STEMMING_DATA_ABS)/%
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer for C#"
	@if test -f '$</voc.txt.gz' ; then \
	  gzip -dc '$</voc.txt.gz' |\
	    $(MONO) csharp_stemwords -l `echo $<|sed 's!.*/!!'` -i /dev/stdin -o tmp.txt; \
	else \
	  $(MONO) csharp_stemwords -l `echo $<|sed 's!.*/!!'` -i $</voc.txt -o tmp.txt; \
	fi
	@if test -f '$</output.txt.gz' ; then \
	  gzip -dc '$</output.txt.gz'|diff -u - tmp.txt; \
	else \
	  diff -u $</output.txt tmp.txt; \
	fi
	@rm tmp.txt

check_pascal: pascal/stemwords
	$(MAKE) do_check_pascal

do_check_pascal: $(ISO_8859_1_algorithms:%=check_pascal_%)

check_pascal_%: $(STEMMING_DATA_ABS)/%
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer with ISO_8859_1 for Pascal"
	@$(ICONV) -fUTF8 -tISO8859-1 '$</voc.txt' |\
	    ./pascal/stemwords -l `echo $<|sed 's!.*/!!'` > tmp.txt
	@$(ICONV) -fUTF8 -tISO8859-1 '$</output.txt' |\
	    diff -u - tmp.txt
	@rm tmp.txt

check_js: $(JS_SOURCES) $(libstemmer_algorithms:%=check_js_%)

# Keep one in $(THIN_FACTOR) entries from gzipped vocabularies.
THIN_FACTOR ?= 3

# Command to thin out the testdata - the full arabic test data causes
# stemwords.js to run out of memory.  Also use for Python tests, which
# take a long time (unless you use pypy).
THIN_TEST_DATA := awk '(FNR % $(THIN_FACTOR) == 0){print}'

check_rust: $(RUST_SOURCES) $(libstemmer_algorithms:%=check_rust_%)

check_rust_%: $(STEMMING_DATA_ABS)/%
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer for Rust"
	@cd rust && if test -f '$</voc.txt.gz' ; then \
	  gzip -dc '$</voc.txt.gz'|$(THIN_TEST_DATA) > tmp.in; \
	  $(cargo) run $(cargoflags) -- -l `echo $<|sed 's!.*/!!'` -i tmp.in -o $(PWD)/tmp.txt; \
	  rm tmp.in; \
	else \
	  $(cargo) run $(cargoflags) -- -l `echo $<|sed 's!.*/!!'` -i $</voc.txt -o $(PWD)/tmp.txt; \
	fi
	@if test -f '$</output.txt.gz' ; then \
	  gzip -dc '$</output.txt.gz'|$(THIN_TEST_DATA)|diff -u - tmp.txt; \
	else \
	  diff -u $</output.txt tmp.txt; \
	fi
	@rm tmp.txt

check_go: $(GO_SOURCES) $(libstemmer_algorithms:%=check_go_%)

check_go_%: $(STEMMING_DATA_ABS)/%
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer for Go"
	@cd go && if test -f '$</voc.txt.gz' ; then \
	  gzip -dc '$</voc.txt.gz'|$(THIN_TEST_DATA) > tmp.in; \
	  $(go) run $(goflags) -l `echo $<|sed 's!.*/!!'` -i tmp.in -o $(PWD)/tmp.txt; \
	  rm tmp.in; \
	else \
	  $(go) run $(goflags) -l `echo $<|sed 's!.*/!!'` -i $</voc.txt -o $(PWD)/tmp.txt; \
	fi
	@if test -f '$</output.txt.gz' ; then \
	  gzip -dc '$</output.txt.gz'|$(THIN_TEST_DATA)|diff -u - tmp.txt; \
	else \
	  diff -u $</output.txt tmp.txt; \
	fi
	@rm tmp.txt

export NODE_PATH = $(js_runtime_dir):$(js_output_dir)

check_js_%: $(STEMMING_DATA)/%
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer for JS"
	@if test -f '$</voc.txt.gz' ; then \
	  gzip -dc '$</voc.txt.gz'|$(THIN_TEST_DATA) > tmp.in; \
	  $(NODE) javascript/stemwords.js -l `echo $<|sed 's!.*/!!'` -i tmp.in -o tmp.txt; \
	  rm tmp.in; \
	else \
	  $(NODE) javascript/stemwords.js -l `echo $<|sed 's!.*/!!'` -i $</voc.txt -o tmp.txt; \
	fi
	@if test -f '$</output.txt.gz' ; then \
	  gzip -dc '$</output.txt.gz'|$(THIN_TEST_DATA)|diff -u - tmp.txt; \
	else \
	  diff -u $</output.txt tmp.txt; \
	fi
	@rm tmp.txt

check_python: check_python_stemwords $(libstemmer_algorithms:%=check_python_%)

check_python_%: $(STEMMING_DATA_ABS)/%
	@echo "Checking output of `echo $<|sed 's!.*/!!'` stemmer for Python"
	@cd python_check && if test -f '$</voc.txt.gz' ; then \
	  gzip -dc '$</voc.txt.gz'|$(THIN_TEST_DATA) > tmp.in; \
	  $(python) stemwords.py -c utf8 -l `echo $<|sed 's!.*/!!'` -i tmp.in -o $(PWD)/tmp.txt; \
	  rm tmp.in; \
	else \
	  $(python) stemwords.py -c utf8 -l `echo $<|sed 's!.*/!!'` -i $</voc.txt -o $(PWD)/tmp.txt; \
	fi
	@if test -f '$</output.txt.gz' ; then \
	  gzip -dc '$</output.txt.gz'|$(THIN_TEST_DATA)|diff -u - tmp.txt; \
	else \
	  diff -u $</output.txt tmp.txt; \
	fi
	@rm tmp.txt

check_python_stemwords: $(PYTHON_STEMWORDS_SOURCE) $(PYTHON_SOURCES)
	mkdir -p python_check
	mkdir -p python_check/snowballstemmer
	cp -a $(PYTHON_RUNTIME_SOURCES) python_check/snowballstemmer
	cp -a $(PYTHON_SOURCES) python_check/snowballstemmer
	cp -a $(PYTHON_STEMWORDS_SOURCE) python_check/

update_version:
	perl -pi -e 's/(SNOWBALL_VERSION.*?)\d+\.\d+\.\d+/$${1}$(SNOWBALL_VERSION)/' \
		compiler/header.h \
		csharp/Snowball/AssemblyInfo.cs \
		python/setup.py

.SUFFIXES: .class .java
