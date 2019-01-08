#!/usr/bin/python

# Copyright (C) Vladimir Prus 2006.
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Tests add-pre-build-hook and add-post-build-hook

import BoostBuild

t = BoostBuild.Tester()

t.write("Jamroot.jam", """
import build-system ;
build-system.add-pre-build-hook pre-build ;
build-system.add-post-build-hook post-build ;

rule pre-build ( )
{
    ECHO "in" pre-build hook ;
}

rule post-build ( okay ? )
{
    ECHO "in" post-build hook $(okay) ;
}

message show : building main targets ;
""")

t.run_build_system(stdout="""\
building main targets
in pre-build hook
...found 1 target...
in post-build hook ok
""")

t.cleanup()
