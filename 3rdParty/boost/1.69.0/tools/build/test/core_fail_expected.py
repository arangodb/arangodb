#!/usr/bin/python

# Copyright 2017 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

import BoostBuild

def test_basic():
    t = BoostBuild.Tester(pass_toolset=0)

    t.write("file.jam", """\
    actions fail
    {
        invalid-dd0eeb5899734622
    }

    FAIL_EXPECTED t1 ;
    fail t1 ;

    UPDATE t1 ;
    """)

    t.run_build_system(["-ffile.jam"])
    t.expect_output_lines("...failed*", False)
    t.expect_nothing_more()

    t.cleanup()
    
def test_error():
    t = BoostBuild.Tester(pass_toolset=0)

    t.write("file.jam", """\
    actions pass
    {
        echo okay >$(<)
    }

    FAIL_EXPECTED t1 ;
    pass t1 ;

    UPDATE t1 ;
    """)

    t.run_build_system(["-ffile.jam"], status=1)
    t.expect_output_lines("...failed pass t1...")
    t.expect_nothing_more()

    t.cleanup()

def test_multiple_actions():
    """FAIL_EXPECTED targets are considered to pass if the first
    updating action fails.  Further actions will be skipped."""
    t = BoostBuild.Tester(pass_toolset=0)

    t.write("file.jam", """\
    actions fail
    {
        invalid-dd0eeb5899734622
    }

    actions pass
    {
         echo okay >$(<)
    }

    FAIL_EXPECTED t1 ;
    fail t1 ;
    pass t1 ;

    UPDATE t1 ;
    """)

    t.run_build_system(["-ffile.jam", "-d1"])
    t.expect_output_lines("...failed*", False)
    t.expect_output_lines("pass t1", False)
    t.expect_nothing_more()

    t.cleanup()

def test_quitquick():
    """Tests that FAIL_EXPECTED targets do not cause early exit
    on failure."""
    t = BoostBuild.Tester(pass_toolset=0)

    t.write("file.jam", """\
    actions fail
    {
        invalid-dd0eeb5899734622
    }

    actions pass
    {
        echo okay >$(<)
    }

    FAIL_EXPECTED t1 ;
    fail t1 ;

    pass t2 ;

    UPDATE t1 t2 ;
    """)

    t.run_build_system(["-ffile.jam", "-q", "-d1"])
    t.expect_output_lines("pass t2")
    t.expect_addition("t2")
    t.expect_nothing_more()

    t.cleanup()
    
def test_quitquick_error():
    """FAIL_EXPECTED targets should cause early exit if they unexpectedly pass."""
    t = BoostBuild.Tester(pass_toolset=0)

    t.write("file.jam", """\
    actions pass
    {
        echo okay >$(<)
    }

    FAIL_EXPECTED t1 ;
    pass t1 ;
    pass t2 ;

    UPDATE t1 t2 ;
    """)

    t.run_build_system(["-ffile.jam", "-q", "-d1"], status=1)
    t.expect_output_lines("pass t2", False)
    t.expect_nothing_more()

    t.cleanup()

test_basic()
test_error()
test_multiple_actions()
test_quitquick()
test_quitquick_error()
