#!/usr/bin/python

# Copyright 2008 Jurko Gospodnetic
# Copyright 2017 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Tests different aspects of Boost Builds automated testing support.

import BoostBuild
import TestCmd

def test_run():
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")

    t.write("Jamroot.jam", """import testing ;
run pass.cpp ;
run fail-compile.cpp ;
run fail-link.cpp ;
run fail-run.cpp ;
""")

    t.run_build_system(status=1)
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.obj")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.exe")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.output")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.run")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.test")

    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.obj")

    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.obj")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.exe")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.output")

    t.expect_nothing_more()

    t.cleanup()

def test_run_fail():
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")

    t.write("Jamroot.jam", """import testing ;
run-fail pass.cpp ;
run-fail fail-compile.cpp ;
run-fail fail-link.cpp ;
run-fail fail-run.cpp ;
""")

    t.run_build_system(status=1)
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.obj")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.exe")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.output")

    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.obj")

    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.obj")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.exe")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.output")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.run")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.test")

    t.expect_nothing_more()

    t.cleanup()

def test_run_change():
    """Tests that the test file is removed when a test fails after it
    previously passed."""

    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int main() { return 1; }\n")
    t.write("fail-compile.cpp", "int main() {}\n")
    t.write("fail-link.cpp", "int main() {}\n")
    t.write("fail-run.cpp", "int main() {}\n")

    t.write("Jamroot.jam", """import testing ;
run-fail pass.cpp ;
run fail-compile.cpp ;
run fail-link.cpp ;
run fail-run.cpp ;
""")
    t.run_build_system()
    # Sanity check
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.test")
    t.expect_addition("bin/fail-compile.test/$toolset/debug*/fail-compile.test")
    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.test")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.test")
    t.expect_output_lines("...failed*", False)

    # Now make them fail
    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")
    t.run_build_system(status=1)

    t.expect_removal("bin/pass.test/$toolset/debug*/pass.test")
    t.expect_removal("bin/fail-compile.test/$toolset/debug*/fail-compile.test")
    t.expect_removal("bin/fail-link.test/$toolset/debug*/fail-link.test")
    t.expect_removal("bin/fail-run.test/$toolset/debug*/fail-run.test")

    t.cleanup()

def test_run_path():
    """Tests that run can find shared libraries even without
    hardcode-dll-paths.  Important: The library is in neither the
    current working directory, nor any system path, nor the same
    directory as the executable, so it should never be found without
    help from Boost.Build."""
    t = BoostBuild.Tester(["hardcode-dll-paths=false"], use_test_config=False)

    t.write("l.cpp", """
void
#if defined(_WIN32)
__declspec(dllexport)
#endif
f() {}
""")
    t.write("pass.cpp", "void f(); int main() { f(); }\n")

    t.write("Jamroot.jam", """import testing ;
lib l : l.cpp : <link>shared ;
run pass.cpp l ;
""")

    t.run_build_system()
    t.expect_addition("bin/$toolset/debug*/l.obj")
    t.expect_addition("bin/$toolset/debug*/l.dll")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.obj")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.exe")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.output")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.run")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.test")

    t.cleanup()

def test_run_args():
    """Tests the handling of args and input-files"""
    t = BoostBuild.Tester(use_test_config=False)
    t.write("test.cpp", """
#include <iostream>
#include <fstream>
int main(int argc, const char ** argv)
{
    for(int i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            std::cout << argv[i] << std::endl;
        }
        else
        {
            std::ifstream ifs(argv[i]);
            std::cout << ifs.rdbuf();
        }
    }
}
""")
    t.write("input1.in", "first input\n")
    t.write("input2.in", "second input\n")
    t.write("Jamroot.jam", """import testing ;
import common ;
# FIXME: The order actually depends on the lexigraphical
# ordering of the virtual target objects, which is just
# crazy.  Switch the order of input1.txt and input2.txt
# to make this fail.  Joining the arguments with && might
# work, but might get a bit complicated to implement as
# dependency properties do not currently support &&.
make input1.txt : input1.in : @common.copy ;
make input2.txt : input2.in : @common.copy ;
run test.cpp : -y -a : input1.txt input2.txt ;
""")
    t.run_build_system()
    t.expect_content("bin/test.test/$toolset/debug*/test.output", """\
-y
-a
first input
second input

EXIT STATUS: 0
""")
    t.cleanup()

def test_link():
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")

    t.write("Jamroot.jam", """import testing ;
link pass.cpp ;
link fail-compile.cpp ;
link fail-link.cpp ;
link fail-run.cpp ;
""")

    t.run_build_system(status=1)
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.obj")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.exe")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.test")

    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.obj")

    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.obj")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.exe")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.test")

    t.expect_nothing_more()

    t.cleanup()

def test_link_fail():
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")

    t.write("Jamroot.jam", """import testing ;
link-fail pass.cpp ;
link-fail fail-compile.cpp ;
link-fail fail-link.cpp ;
link-fail fail-run.cpp ;
""")

    t.run_build_system(status=1)
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.obj")

    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.obj")
    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.exe")
    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.test")

    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.obj")

    t.expect_nothing_more()

    t.cleanup()

def test_link_change():
    """Tests that the test file is removed when a test fails after it
    previously passed."""

    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-compile.cpp", "int main() {}\n")
    t.write("fail-link.cpp", "int main() {}\n")

    t.write("Jamroot.jam", """import testing ;
link-fail pass.cpp ;
link fail-compile.cpp ;
link fail-link.cpp ;
""")
    t.run_build_system()
    # Sanity check
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.test")
    t.expect_addition("bin/fail-compile.test/$toolset/debug*/fail-compile.test")
    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.test")
    t.expect_output_lines("...failed*", False)

    # Now make them fail
    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.run_build_system(status=1)

    t.expect_removal("bin/pass.test/$toolset/debug*/pass.test")
    t.expect_removal("bin/fail-compile.test/$toolset/debug*/fail-compile.test")
    t.expect_removal("bin/fail-link.test/$toolset/debug*/fail-link.test")

    t.cleanup()

def test_compile():
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")

    t.write("Jamroot.jam", """import testing ;
compile pass.cpp ;
compile fail-compile.cpp ;
compile fail-link.cpp ;
compile fail-run.cpp ;
""")

    t.run_build_system(status=1)
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.obj")
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.test")

    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.obj")
    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.test")

    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.obj")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.test")

    t.expect_nothing_more()

    t.cleanup()

def test_compile_fail():
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")

    t.write("Jamroot.jam", """import testing ;
compile-fail pass.cpp ;
compile-fail fail-compile.cpp ;
compile-fail fail-link.cpp ;
compile-fail fail-run.cpp ;
""")

    t.run_build_system(status=1)
    t.expect_addition("bin/fail-compile.test/$toolset/debug*/fail-compile.obj")
    t.expect_addition("bin/fail-compile.test/$toolset/debug*/fail-compile.test")

    t.expect_nothing_more()

    t.cleanup()

def test_compile_change():
    """Tests that the test file is removed when a test fails after it
    previously passed."""

    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass.cpp", "#error expected to fail\n")
    t.write("fail-compile.cpp", "int main() {}\n")

    t.write("Jamroot.jam", """import testing ;
compile-fail pass.cpp ;
compile fail-compile.cpp ;
""")
    t.run_build_system()
    # Sanity check
    t.expect_addition("bin/pass.test/$toolset/debug*/pass.test")
    t.expect_addition("bin/fail-compile.test/$toolset/debug*/fail-compile.test")
    t.expect_output_lines("...failed*", False)

    # Now make them fail
    t.write("pass.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.run_build_system(status=1)

    t.expect_removal("bin/pass.test/$toolset/debug*/pass.test")
    t.expect_removal("bin/fail-compile.test/$toolset/debug*/fail-compile.test")

    t.cleanup()

def test_remove_test_targets(option):
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass-compile.cpp", "int main() {}\n")
    t.write("pass-link.cpp", "int main() {}\n")
    t.write("pass-run.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")
    t.write("source.cpp", "int f();\n")

    t.write("Jamroot.jam", """import testing ;
obj source.o : source.cpp ;
compile pass-compile.cpp ;
link pass-link.cpp source.o ;
run pass-run.cpp source.o ;
compile-fail fail-compile.cpp ;
link-fail fail-link.cpp ;
run-fail fail-run.cpp ;
""")

    t.run_build_system([option])

    t.expect_addition("bin/$toolset/debug*/source.obj")

    t.expect_addition("bin/pass-compile.test/$toolset/debug*/pass-compile.test")

    t.expect_addition("bin/pass-link.test/$toolset/debug*/pass-link.test")

    t.expect_addition("bin/pass-run.test/$toolset/debug*/pass-run.output")
    t.expect_addition("bin/pass-run.test/$toolset/debug*/pass-run.run")
    t.expect_addition("bin/pass-run.test/$toolset/debug*/pass-run.test")

    t.expect_addition("bin/fail-compile.test/$toolset/debug*/fail-compile.test")

    t.expect_addition("bin/fail-link.test/$toolset/debug*/fail-link.test")
    
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.output")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.run")
    t.expect_addition("bin/fail-run.test/$toolset/debug*/fail-run.test")

    t.expect_nothing_more()

    t.cleanup()

def test_dump_tests():
    """Tests the output of the --dump-tests option"""
    t = BoostBuild.Tester(use_test_config=False)

    t.write("pass-compile.cpp", "int main() {}\n")
    t.write("pass-link.cpp", "int main() {}\n")
    t.write("pass-run.cpp", "int main() {}\n")
    t.write("fail-compile.cpp", "#error expected to fail\n")
    t.write("fail-link.cpp", "int f();\nint main() { return f(); }\n")
    t.write("fail-run.cpp", "int main() { return 1; }\n")

    t.write("Jamroot.jam", """import testing ;
run pass-run.cpp ;
run-fail fail-run.cpp ;
link pass-link.cpp ;
link-fail fail-link.cpp ;
compile pass-compile.cpp ;
compile-fail fail-compile.cpp ;
build-project libs/any/test ;
build-project libs/any/example ;
build-project libs/any ;
build-project tools/bcp/test ;
build-project tools/bcp/example ;
build-project subdir/test ;
build-project status ;
build-project outside/project ;
""")
    def write_subdir(dir):
        t.write(dir + "/test.cpp", "int main() {}\n")
        t.write(dir + "/Jamfile", "run test.cpp ;")
    write_subdir("libs/any/test")
    write_subdir("libs/any/example")
    write_subdir("libs/any")
    write_subdir("tools/bcp/test")
    write_subdir("tools/bcp/example")
    write_subdir("status")
    write_subdir("subdir/test")
    t.write("outside/other/test.cpp", "int main() {}\n")
    t.write("outside/project/Jamroot", "run ../other/test.cpp ;")
    t.run_build_system(["--dump-tests", "-n", "-d0"],
                       match=TestCmd.match_re, stdout=
"""boost-test\(RUN\) ".*/pass-run" : "pass-run\.cpp"
boost-test\(RUN_FAIL\) ".*/fail-run" : "fail-run\.cpp"
boost-test\(LINK\) ".*/pass-link" : "pass-link\.cpp"
boost-test\(LINK_FAIL\) ".*/fail-link" : "fail-link\.cpp"
boost-test\(COMPILE\) ".*/pass-compile" : "pass-compile\.cpp"
boost-test\(COMPILE_FAIL\) ".*/fail-compile" : "fail-compile\.cpp"
boost-test\(RUN\) "any/test" : "libs/any/test\.cpp"
boost-test\(RUN\) "any/test" : "libs/any/test/test\.cpp"
boost-test\(RUN\) "any/test" : "libs/any/example/test\.cpp"
boost-test\(RUN\) "bcp/test" : "tools/bcp/test/test\.cpp"
boost-test\(RUN\) "bcp/test" : "tools/bcp/example/test\.cpp"
boost-test\(RUN\) ".*/subdir/test/test" : "subdir/test/test\.cpp"
boost-test\(RUN\) "test" : "status/test\.cpp"
boost-test\(RUN\) ".*/outside/project/test" : "../other/test.cpp"
""")
    t.cleanup()

################################################################################
#
# test_files_with_spaces_in_their_name()
# --------------------------------------
#
################################################################################

def test_files_with_spaces_in_their_name():
    """Regression test making sure test result files get created correctly when
    testing files with spaces in their name.
    """

    t = BoostBuild.Tester(use_test_config=False)

    t.write("valid source.cpp", "int main() {}\n");

    t.write("invalid source.cpp", "this is not valid source code");

    t.write("jamroot.jam", """
import testing ;
testing.compile "valid source.cpp" ;
testing.compile-fail "invalid source.cpp" ;
""")

    t.run_build_system(status=0)
    t.expect_addition("bin/invalid source.test/$toolset/debug*/invalid source.obj")
    t.expect_addition("bin/invalid source.test/$toolset/debug*/invalid source.test")
    t.expect_addition("bin/valid source.test/$toolset/debug*/valid source.obj")
    t.expect_addition("bin/valid source.test/$toolset/debug*/valid source.test")

    t.expect_content("bin/valid source.test/$toolset/debug*/valid source.test", \
        "passed" )
    t.expect_content( \
        "bin/invalid source.test/$toolset/debug*/invalid source.test", \
        "passed" )
    t.expect_content( \
        "bin/invalid source.test/$toolset/debug*/invalid source.obj", \
        "failed as expected" )

    t.cleanup()


################################################################################
#
# main()
# ------
#
################################################################################

test_run()
test_run_fail()
test_run_change()
test_run_path()
test_run_args()
test_link()
test_link_fail()
test_link_change()
test_compile()
test_compile_fail()
test_compile_change()
test_remove_test_targets("--remove-test-targets")
test_remove_test_targets("preserve-test-targets=off")
test_dump_tests()
test_files_with_spaces_in_their_name()
