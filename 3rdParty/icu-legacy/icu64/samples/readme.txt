## Copyright (C) 2016 and later: Unicode, Inc. and others.
## License & terms of use: http://www.unicode.org/copyright.html
##
## Copyright (c) 2002-2010, International Business Machines Corporation 
## and others. All Rights Reserved.

This directory contains sample code using ICU4C routines. Below is a
short description of the contents of this directory.

coll    - shows how collation compares strings

strsrch - demonstrates how to search for patterns in Unicode text using the usearch interface.

translit - demonstrates the use of ICU transliteration

ugrep    - demonstrates ICU Regular Expressions. 


==
* Where can I find more sample code?

 - The "uconv" utility is a full-featured command line application.
   It is normally built with ICU, and is located in icu/source/extra/uconv

 - The "icu-demos" contains other applications and libraries not
   included with ICU.  You can check it out from https://github.com/unicode-org/icu-demos
   using github clone. See the README file for additional information.

==
* How do I build the samples?

 - See the Readme in each subdirectory

 To build all samples at once:

    Windows MSVC:   
            - build ICU
	    - open 'all' project file in 'all' subdirectory
            - build project
            - sample executables will be located in /x86/Debug folders of each sample subdirectory

    Unix:   - build and install (make install) ICU
            - be sure 'icu-config' is accessible from the PATH
            - type 'make all-samples' from this directory 
               (other targets:  clean-samples, check-samples)
      Note: 'make all-samples' won't work correctly in out of source builds.
