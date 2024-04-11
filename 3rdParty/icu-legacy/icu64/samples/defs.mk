# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html
#
# Copyright (c) 2002-2012 IBM, Inc. and others
# Sample code makefile definitions 

CLEANFILES=*~ $(TARGET).out
####################################################################
# Load ICU information. You can copy this to other makefiles #######
####################################################################
CC=$(shell icu-config --cc)
CXX=$(shell icu-config --cxx)
CPPFLAGS=$(shell icu-config --cppflags)
CFLAGS=$(shell icu-config --cflags)
CXXFLAGS=$(shell icu-config --cxxflags)
LDFLAGS =$^ $(shell icu-config --ldflags)
LDFLAGS_USTDIO =$(shell icu-config --ldflags-icuio)
INVOKE=$(shell icu-config --invoke)
GENRB=$(shell icu-config --invoke=genrb)
GENRBOPT=
PKGDATA=$(shell icu-config --invoke=pkgdata)
SO=$(shell icu-config --so)
PKGDATAOPTS=-r $(shell icu-config --version) -w -v -d .
# default - resources in same mode as ICU
RESMODE=$(shell icu-config --icudata-mode)

####################################################################
### Project independent things (common) 
### We depend on gmake for the bulk of the work 

RMV=rm -rf
