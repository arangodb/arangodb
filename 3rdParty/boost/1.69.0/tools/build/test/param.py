#!/usr/bin/python

# Copyright 2018 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import BoostBuild

t = BoostBuild.Tester(pass_toolset=0)

t.write("Jamroot.jam", """\
import param ;
import assert ;
import errors : try catch ;
rule test1 ( )
{
    param.handle-named-params ;
}
test1 ;
rule test2 ( sources * )
{
    param.handle-named-params sources ;
    return $(sources) ;
}
assert.result : test2 ;
assert.result test1.cpp test2.cpp : test2 test1.cpp test2.cpp ;
assert.result test1.cpp test2.cpp : test2 sources test1.cpp test2.cpp ;
rule test3 ( sources * : requirements * )
{
    param.handle-named-params sources requirements ;
    return $(sources) -- $(requirements) ;
}
assert.result -- : test3 ;
assert.result -- <link>shared : test3 : <link>shared ;
assert.result test1.cpp -- <link>shared : test3 test1.cpp : <link>shared ;
assert.result test1.cpp -- <link>shared
  : test3 test1.cpp : requirements <link>shared ;
assert.result test1.cpp -- <link>shared
  : test3 sources test1.cpp : requirements <link>shared ;
assert.result test1.cpp -- <link>shared
  : test3 requirements <link>shared : sources test1.cpp ;
assert.result -- : test3 sources ;
assert.result -- : test3 requirements ;
assert.result -- <link>shared : test3 requirements <link>shared ;
try ;
{
    test3 sources test1.cpp : sources test2.cpp ;
}
catch Parameter 'sources' passed more than once. ;
try ;
{
    test3 sources test1.cpp : <link>shared ;
}
catch "Positional arguments must appear first." ;
EXIT : 0 ;
""")

t.run_build_system()

t.cleanup()
