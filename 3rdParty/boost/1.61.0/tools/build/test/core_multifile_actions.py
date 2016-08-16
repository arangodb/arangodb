#!/usr/bin/python

# Copyright 2013 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

#   Tests that actions that produce multiple targets are handled
# correctly.  The rules are as follows:
#
# - If any action that updates a target is run, then the target
#   is considered to be out-of-date and all of its updating actions
#   are run in order.
# - A target is considered updated when all of its updating actions
#   have completed successfully.
# - If any updating action for a target fails, then the remaining
#   actions are skipped and the target is marked as failed.
#
# Note that this is a more thorough test case for the same
# problem that core_parallel_multifile_actions_N.py checks for.

import BoostBuild

t = BoostBuild.Tester(pass_toolset=0, pass_d0=False)

t.write("file.jam", """
actions update
{
    echo updating $(<)
}

update x1 x2 ;
update x2 x3 ;
""")

# Updating x1 should force x2 to update as well.
t.run_build_system(["-ffile.jam", "x1"], stdout="""\
...found 3 targets...
...updating 3 targets...
update x1
updating x1 x2
update x2
updating x2 x3
...updated 3 targets...
""")

# If x1 is up-to-date, we don't need to update x2,
# even though x2 is missing.
t.write("x1", "")
t.run_build_system(["-ffile.jam", "x1"], stdout="""\
...found 1 target...
""")

# Building x3 should update x1 and x2, even though
# x1 would be considered up-to-date, taken alone.
t.run_build_system(["-ffile.jam", "x3"], stdout="""\
...found 3 targets...
...updating 2 targets...
update x1
updating x1 x2
update x2
updating x2 x3
...updated 3 targets...
""")

# Updating x2 should succeed, but x3 should be skipped
t.rm("x1")
t.write("file.jam", """\
actions update
{
    echo updating $(<)
}
actions fail
{
    echo failed $(<)
    exit 1
}

update x1 x2 ;
fail x1 ;
update x1 x3 ;
update x2 ;
update x3 ;
""")

t.run_build_system(["-ffile.jam", "x3"], status=1, stdout="""\
...found 3 targets...
...updating 3 targets...
update x1
updating x1 x2
fail x1
failed x1

    echo failed x1
    exit 1

...failed fail x1...
update x2
updating x2
...failed updating 2 targets...
...updated 1 target...
""")

# Make sure that dependencies of targets that are
# updated as a result of a multifile action are
# processed correctly.
t.rm("x1")
t.write("file.jam", """\
actions update
{
    echo updating $(<)
}

update x1 ;
update x2 ;
DEPENDS x2 : x1 ;
update x2 x3 ;
""")
t.run_build_system(["-ffile.jam", "x3"], stdout="""\
...found 3 targets...
...updating 3 targets...
update x1
updating x1
update x2
updating x2
update x2
updating x2 x3
...updated 3 targets...
""")

# JAM_SEMAPHORE rules:
#
# - if two updating actions have targets that share a semaphore,
#   these actions cannot be run in parallel.
#
t.write("file.jam", """\
actions update
{
    echo updating $(<)
}

targets = x1 x2 ;
JAM_SEMAPHORE on $(targets) = <s>update_sem ;
update x1 x2 ;
""")
t.run_build_system(["-ffile.jam", "x1"], stdout="""\
...found 2 targets...
...updating 2 targets...
update x1
updating x1 x2
...updated 2 targets...
""")

# A target can appear multiple times in an action
t.write("file.jam", """\
actions update
{
    echo updating $(<)
}

update x1 x1 ;
""")
t.run_build_system(["-ffile.jam", "x1"], stdout="""\
...found 1 target...
...updating 1 target...
update x1
updating x1 x1
...updated 1 target...
""")

# Together actions should check that all the targets are the same
# before combining.
t.write("file.jam", """\
actions together update
{
    echo updating $(<) : $(>)
}

update x1 x2 : s1 ;
update x1 x2 : s2 ;

update x3 : s3 ;
update x3 x4 : s4 ;
update x4 x3 : s5 ;
DEPENDS all : x1 x2 x3 x4 ;
""")
t.run_build_system(["-ffile.jam"], stdout="""\
...found 5 targets...
...updating 4 targets...
update x1
updating x1 x2 : s1 s2
update x3
updating x3 : s3
update x3
updating x3 x4 : s4
update x4
updating x4 x3 : s5
...updated 4 targets...
""")



t.cleanup()
