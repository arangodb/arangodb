#!/bin/sh
# Copyright (C) 2020 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# run the test with 1, 10, 100, 1000, 10000, 100000 iterations
LD_LIBRARY_PATH=lib:stubdata:tools/ctestfw:../../lib:../../stubdata:../../tools/ctestfw:$LD_LIBRARY_PATH:../../../lib:../../../stubdata:../../../tools/ctestfw:$LD_LIBRARY_PATH \
  ./localecanperf TestLocaleCreateCanonical -i 1

LD_LIBRARY_PATH=lib:stubdata:tools/ctestfw:../../lib:../../stubdata:../../tools/ctestfw:$LD_LIBRARY_PATH:../../../lib:../../../stubdata:../../../tools/ctestfw:$LD_LIBRARY_PATH \
  ./localecanperf TestLocaleCreateCanonical -i 10

LD_LIBRARY_PATH=lib:stubdata:tools/ctestfw:../../lib:../../stubdata:../../tools/ctestfw:$LD_LIBRARY_PATH:../../../lib:../../../stubdata:../../../tools/ctestfw:$LD_LIBRARY_PATH \
  ./localecanperf TestLocaleCreateCanonical -i 100

LD_LIBRARY_PATH=lib:stubdata:tools/ctestfw:../../lib:../../stubdata:../../tools/ctestfw:$LD_LIBRARY_PATH:../../../lib:../../../stubdata:../../../tools/ctestfw:$LD_LIBRARY_PATH \
  ./localecanperf TestLocaleCreateCanonical -i 1000

LD_LIBRARY_PATH=lib:stubdata:tools/ctestfw:../../lib:../../stubdata:../../tools/ctestfw:$LD_LIBRARY_PATH:../../../lib:../../../stubdata:../../../tools/ctestfw:$LD_LIBRARY_PATH \
  ./localecanperf TestLocaleCreateCanonical -i 10000

LD_LIBRARY_PATH=lib:stubdata:tools/ctestfw:../../lib:../../stubdata:../../tools/ctestfw:$LD_LIBRARY_PATH:../../../lib:../../../stubdata:../../../tools/ctestfw:$LD_LIBRARY_PATH \
  ./localecanperf TestLocaleCreateCanonical -i 100000
