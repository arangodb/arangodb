#!/usr/bin/python

# Copyright 2013 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Tests that action sources are not reordered

import BoostBuild

t = BoostBuild.Tester()

t.write("check-order.jam", """\
import type ;
import generators ;

type.register ORDER_TEST : order-test ;

SPACE = " " ;
nl = "\n" ;
actions check-order
{
    echo$(SPACE)$(>[1])>$(<[1])
    echo$(SPACE)$(>[2-])>>$(<[1])$(nl)
}

generators.register-composing check-order.check-order : C : ORDER_TEST ;
""")

# The aliases are necessary for this test, since
# the targets were sorted by virtual target
# id, not by file name.
t.write("jamroot.jam", """\
import check-order ;
alias file1 : file1.c ;
alias file2 : file2.c ;
alias file3 : file3.c ;
order-test check : file2 file1 file3 ;
""")

t.write("file1.c", "")
t.write("file2.c", "")
t.write("file3.c", "")

t.run_build_system()
t.expect_addition("bin/$toolset/debug/check.order-test")
t.expect_content("bin/$toolset/debug/check.order-test", """\
file2.c
file1.c
file3.c
""", True)

t.cleanup()
