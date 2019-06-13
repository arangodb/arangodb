#!/usr/bin/python
#
# Copyright (c) 2008 Steven Watanabe
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt) or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import BoostBuild

def test_default_order():
    tester = BoostBuild.Tester(use_test_config=False)
    tester.write("jamroot.jam", """

    import order ;
    import "class" : new ;

    obj test : test.cpp : <include>b <include>a ;
    """)

    tester.write("test.cpp", """
    #include <test.hpp>
    int main() { f(); }
    """)
    
    tester.write("a/test.hpp", """
    void f();
    """)
    
    tester.write("b/test.hpp", """
    """)
    
    tester.run_build_system()
    
    tester.expect_addition("bin/$toolset/debug*/test.obj")
    
    # Check that the dependencies are correct
    tester.touch("a/test.hpp")
    tester.run_build_system()
    tester.expect_touch("bin/$toolset/debug*/test.obj")
    tester.expect_nothing_more()
    
    tester.touch("b/test.hpp")
    tester.run_build_system()
    tester.expect_nothing_more()

    tester.cleanup()

def test_default_order_mixed():
    tester = BoostBuild.Tester(use_test_config=False)
    tester.write("jamroot.jam", """

    import order ;
    import "class" : new ;

    obj test : test.cpp : <include>b <include>a <include>c&&d ;
    """)

    tester.write("test.cpp", """
    #include <test.hpp>
    int main() { f(); }
    """)
    
    tester.write("a/test.hpp", """
    void f();
    """)
    
    tester.write("b/test.hpp", """
    """)
    
    tester.run_build_system()
    
    tester.expect_addition("bin/$toolset/debug*/test.obj")
    
    # Check that the dependencies are correct
    tester.touch("a/test.hpp")
    tester.run_build_system()
    tester.expect_touch("bin/$toolset/debug*/test.obj")
    tester.expect_nothing_more()
    
    tester.touch("b/test.hpp")
    tester.run_build_system()
    tester.expect_nothing_more()

    tester.cleanup()

def test_basic():
    tester = BoostBuild.Tester(use_test_config=False)
    tester.write("jamroot.jam", """
    obj test : test.cpp : <include>a&&b ;
    """)

    tester.write("test.cpp", """
    #include <test1.hpp>
    #include <test2.hpp>
    int main() {}
    """)

    tester.write("a/test1.hpp", """
    """)

    tester.write("b/test2.hpp", """
    """)

    tester.run_build_system()

    tester.expect_addition("bin/$toolset/debug*/test.obj")

    # Check that the dependencies are correct
    tester.touch("a/test1.hpp")
    tester.run_build_system()
    tester.expect_touch("bin/$toolset/debug*/test.obj")

    tester.touch("b/test2.hpp")
    tester.run_build_system()
    tester.expect_touch("bin/$toolset/debug*/test.obj")

    tester.cleanup()

def test_order1():
    t = BoostBuild.Tester(use_test_config=False)
    t.write("jamroot.jam", """
    obj test : test.cpp : <include>a&&b ;
    """)
    t.write("test.cpp", """
    #include <test.h>
    int main() {}
    """)
    t.write("a/test.h", """
    """)
    t.write("b/test.h", """
    #error should find a/test.h
    """)
    t.run_build_system()

    t.touch("a/test.h")
    t.run_build_system()
    t.expect_touch("bin/$toolset/debug*/test.obj")
    t.expect_nothing_more()

    t.touch("b/test.h")
    t.run_build_system()
    t.expect_nothing_more()

    t.cleanup()

def test_order2():
    t = BoostBuild.Tester(use_test_config=False)
    t.write("jamroot.jam", """
    obj test : test.cpp : <include>b&&a ;
    """)
    t.write("test.cpp", """
    #include <test.h>
    int main() {}
    """)
    t.write("a/test.h", """
    #error should find b/test.h
    """)
    t.write("b/test.h", """
    """)
    t.run_build_system()

    t.touch("a/test.h")
    t.run_build_system()
    t.expect_nothing_more()

    t.touch("b/test.h")
    t.run_build_system()
    t.expect_touch("bin/$toolset/debug*/test.obj")
    t.expect_nothing_more()

    t.cleanup()

def test_order_graph():
    t = BoostBuild.Tester(use_test_config=False)
    t.write("jamroot.jam", """
    obj test : test.cpp :
        <include>b&&a
        <include>c&&b
        <include>a
        <include>c
        <include>b
        <include>e&&b&&d
      ;
    """)
    t.write("test.cpp", """
    #include <test1.h>
    #include <test2.h>
    #include <test3.h>
    #include <test4.h>
    int main() {}
    """)
    t.write("b/test1.h", "")
    t.write("a/test1.h", "#error should find b/test1.h\n")

    t.write("c/test2.h", "")
    t.write("b/test2.h", "#error should find c/test2.h\n")

    t.write("e/test3.h", "")
    t.write("b/test3.h", "#error should find e/test3.h\n")

    t.write("b/test4.h", "")
    t.write("d/test4.h", "#error should find b/test4.h\n")

    t.run_build_system()
    t.expect_addition("bin/$toolset/debug*/test.obj")

    t.touch("b/test1.h")
    t.run_build_system()
    t.expect_touch("bin/$toolset/debug*/test.obj")
    t.expect_nothing_more()

    t.touch("a/test1.h")
    t.run_build_system()
    t.expect_nothing_more()

    t.touch("c/test2.h")
    t.run_build_system()
    t.expect_touch("bin/$toolset/debug*/test.obj")
    t.expect_nothing_more()

    t.touch("b/test2.h")
    t.run_build_system()
    t.expect_nothing_more()

    t.touch("e/test3.h")
    t.run_build_system()
    t.expect_touch("bin/$toolset/debug*/test.obj")
    t.expect_nothing_more()

    t.touch("b/test3.h")
    t.run_build_system()
    t.expect_nothing_more()

    t.touch("b/test4.h")
    t.run_build_system()
    t.expect_touch("bin/$toolset/debug*/test.obj")
    t.expect_nothing_more()

    t.touch("d/test4.h")
    t.run_build_system()
    t.expect_nothing_more()

    t.cleanup()

test_default_order()
test_default_order_mixed()
test_basic()
test_order1()
test_order2()
test_order_graph()
