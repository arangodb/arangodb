#!/usr/bin/python

# Copyright 2018 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Tests the parsing of tokens

import BoostBuild

t = BoostBuild.Tester(pass_toolset=0)

t.write("file.jam", """\
rule test1 ( args * )
{
    EXIT $(args) : 0 ;
}

test1
a # a comment
# another comment
b
c #| a multiline comment |# d
#| another
multiline
comment
|#
e "#f" ;
""")

t.run_build_system(["-ffile.jam"], stdout="""\
a b c d e #f
""")

t.cleanup()
