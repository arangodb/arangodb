#!/usr/bin/python

# Copyright 2015 Aaron Boman
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Test that free property inside.

import BoostBuild

t = BoostBuild.Tester(use_test_config=False)

t.write("jamroot.jam", "")
t.write(
    "subdir/build.jam",
    """
    import feature ;
    feature.feature my-feature : : free ;
    """
)
t.write(
    "subdir/subsubdir/build.jam",
    """
    exe hello : hello.c ;
    """
)
t.write(
    "subdir/subsubdir/hello.c",
    r"""
    #include <stdio.h>

    int main(int argc, char **argv){
        printf("%s\n", "Hello, World!");
    }
    """
)

# run from the root directory
t.run_build_system(['subdir/subsubdir', 'my-feature="some value"'])

t.cleanup()
