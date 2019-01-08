#!/usr/bin/python
#
# Copyright 2018 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Tests the mapping of various suffixes
# In particular, .so[.version] needs to
# be mapped as a SHARED_LIB.

import BoostBuild

t = BoostBuild.Tester()

t.write("Jamroot.jam", """\
import type : type ;
ECHO [ type source.c ] ;
ECHO [ type source.cc ] ;
ECHO [ type source.cxx ] ;
ECHO [ type source.cpp ] ;
ECHO [ type source.o ] ;
ECHO [ type source.obj ] ;
ECHO [ type boost_system.lib ] ;
ECHO [ type boost_system.so ] ;
ECHO [ type boost_system.dll ] ;
EXIT [ type boost_system.so.1.66.0 ] : 0 ;
""")

t.run_build_system(stdout="""\
C
CPP
CPP
CPP
OBJ
OBJ
STATIC_LIB
SHARED_LIB
SHARED_LIB
SHARED_LIB
""")

t.cleanup()
