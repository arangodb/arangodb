# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html
# Copyright (C) 2016 International Business Machines Corporation
# and others. All rights reserved.
#
# Run this script from $ICU_ROOT/src/source/
# ~/svn.icu/trunk/src/source$  test/hdrtst/testinternalheaders.sh

CC=clang
CXX=clang++

ERROR_EXIT=0

# Runtime libraries

for file in `ls common/*.h`; do
    echo $file
    echo '#include "'$file'"' > ht_temp.cpp ;
    echo 'void noop() {}' >> ht_temp.cpp ;
    $CXX -c -std=c++17 -I common -DU_COMMON_IMPLEMENTATION -O0 ht_temp.cpp ;
    if [ $? != 0 ] ; then
        ERROR_EXIT=1
    fi
done ;

for file in `ls i18n/*.h`; do
    echo $file
    echo '#include "'$file'"' > ht_temp.cpp ;
    echo 'void noop() {}' >> ht_temp.cpp ;
    $CXX -c -std=c++17 -I common -I i18n -DU_I18N_IMPLEMENTATION -O0 ht_temp.cpp ;
    if [ $? != 0 ] ; then
        ERROR_EXIT=1
    fi
done ;

for file in `ls io/*.h`; do
    echo $file
    echo '#include "'$file'"' > ht_temp.cpp ;
    echo 'void noop() {}' >> ht_temp.cpp ;
    $CXX -c -std=c++17 -I common -I i18n -I io -DU_IO_IMPLEMENTATION -O0 ht_temp.cpp ;
    if [ $? != 0 ] ; then
        ERROR_EXIT=1
    fi
done ;

# layout is removed.

# layoutex now depends on external additions such as HarfBuzz, skip here

# -I .  for includes of layout/*.h
#for file in `ls layoutex/*.h`; do
#    echo $file
#    echo '#include "'$file'"' > ht_temp.cpp ;
#    echo 'void noop() {}' >> ht_temp.cpp ;
#    $CXX -c -I common -I i18n -I io -I layout -I . -I layoutex -O0 ht_temp.cpp ;
#done ;

# Tools

for file in `ls tools/toolutil/*.h`; do
    echo $file
    echo '#include "'$file'"' > ht_temp.cpp ;
    echo 'void noop() {}' >> ht_temp.cpp ;
    $CXX -c -std=c++17 -I common -I i18n -I io -I tools/toolutil -O0 ht_temp.cpp ;
    if [ $? != 0 ] ; then
        ERROR_EXIT=1
    fi
done ;

# Exclude tzcode: tools/tzcode/private.h uses an argument "new" in a function declaration.
# Markus sent an email to the tz list on 20160307 requesting that it be renamed.
# We don't want to patch it, and don't want to spend the time for this script here
# to know about C-only header files.

for tool in escapesrc genccode gencmn gencolusb gennorm2 genren gentest icupkg icuswap \
        pkgdata genbrk gencfu gencnval gendict genrb gensprep icuinfo makeconv memcheck; do
    for file in `ls tools/$tool/*.h`; do
        echo $file
        echo '#include "'$file'"' > ht_temp.cpp ;
        echo 'void noop() {}' >> ht_temp.cpp ;
        $CXX -c -std=c++17 -I common -I i18n -I io -I tools/toolutil -I tools/$tool -O0 ht_temp.cpp ;
        if [ $? != 0 ] ; then
            ERROR_EXIT=1
        fi
    done ;
done ;

# Tests

for file in `ls tools/ctestfw/unicode/*.h`; do
    echo $file
    echo '#include "'$file'"' > ht_temp.cpp ;
    echo 'void noop() {}' >> ht_temp.cpp ;
    $CXX -c -std=c++17 -I common -I i18n -I io -I tools/toolutil -I tools/ctestfw -O0 ht_temp.cpp ;
    if [ $? != 0 ] ; then
        ERROR_EXIT=1
    fi
done ;

# C not C++ for cintltst
for file in `ls test/cintltst/*.h`; do
    echo $file
    echo '#include "'$file'"' > ht_temp.c ;
    echo 'void noop() {}' >> ht_temp.c ;
    $CC -c -std=c11 -I common -I i18n -I io -I tools/toolutil -I tools/ctestfw -I test/cintltst -O0 ht_temp.c ;
    if [ $? != 0 ] ; then
        ERROR_EXIT=1
    fi
done ;

for test in intltest iotest testmap thaitest fuzzer; do
    for file in `ls test/$test/*.h`; do
        echo $file
        echo '#include "'$file'"' > ht_temp.cpp ;
        echo 'void noop() {}' >> ht_temp.cpp ;
        $CXX -c -std=c++17 -I common -I i18n -I io -I tools/toolutil -I tools/ctestfw -I test/$test -O0 ht_temp.cpp ;
        if [ $? != 0 ] ; then
            ERROR_EXIT=1
        fi
    done ;
done ;

# layoutex now depends on external additions such as HarfBuzz, skip here

#for file in `ls test/letest/*.h`; do
#    echo $file
#    echo '#include "'$file'"' > ht_temp.cpp ;
#    echo 'void noop() {}' >> ht_temp.cpp ;
#    $CXX -c -I common -I i18n -I io -I layout -I . -I layoutex -I tools/toolutil -I tools/ctestfw -I test/letest -O0 ht_temp.cpp ;
#done ;

# TODO: perf/*/*.h

rm ht_temp.cpp ht_temp.c ht_temp.o

echo $ERROR_EXIT
exit $ERROR_EXIT
