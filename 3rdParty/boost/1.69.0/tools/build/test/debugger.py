#!/usr/bin/python

# Copyright 2016 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Test for the debugger

import BoostBuild
import TestCmd
import re

def split_stdin_stdout(text):
    """stdin is all text after the prompt up to and including
    the next newline.  Everything else is stdout.  stdout
    may contain regular expressions enclosed in {{}}."""
    prompt = re.escape('(b2db) ')
    pattern = re.compile('(?<=%s)(.*\n)' % prompt)
    text = text.replace("{{bjam}}", "{{.*}}bjam{{(?:\\.exe)?}}")
    stdin = ''.join(re.findall(pattern, text))
    stdout = re.sub(pattern, '', text)
    outside_pattern = re.compile(r'(?:\A|(?<=\}\}))(?:[^\{]|(?:\{(?!\{)))*(?:(?=\{\{)|\Z)')

    def escape_line(line):
        line = re.sub(outside_pattern, lambda m: re.escape(m.group(0)), line)
        return re.sub(r'\{\{|\}\}', '', line)

    stdout = '\n'.join([escape_line(line) for line in stdout.split('\n')])
    return (stdin,stdout)

def run(tester, io):
    (input,output) = split_stdin_stdout(io)
    tester.run_build_system(stdin=input, stdout=output, match=TestCmd.match_re)

def make_tester():
    return BoostBuild.Tester(["-dconsole"], pass_toolset=False, pass_d0=False,
        use_test_config=False, ignore_toolset_requirements=False, match=TestCmd.match_re)

def test_run():
    t = make_tester()
    t.write("test.jam", """\
        UPDATE ;
    """)

    run(t, """\
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Child {{\d+}} exited with status 0
(b2db) quit
""")

    t.cleanup()

def test_exit_status():
    t = make_tester()
    t.write("test.jam", """\
        EXIT : 1 ;
    """)
    run(t, """\
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam

Child {{\d+}} exited with status 1
(b2db) quit
""")
    t.cleanup()

def test_step():
    t = make_tester()
    t.write("test.jam", """\
        rule g ( )
        {
            a = 1 ;
            b = 2 ;
        }
        rule f ( )
        {
            g ;
            c = 3 ;
        }
        f ;
    """)
    run(t, """\
(b2db) break f
Breakpoint 1 set at f
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( ) at test.jam:8
8	            g ;
(b2db) step
3	            a = 1 ;
(b2db) step
4	            b = 2 ;
(b2db) step
9	            c = 3 ;
(b2db) quit
""")
    t.cleanup()

# Note: step doesn't need to worry about breakpoints,
# as it always stops at the next line executed.

def test_next():
    t = make_tester()
    t.write("test.jam", """\
        rule g ( )
        {
            a = 1 ;
        }
        rule f ( )
        {
            g ;
            b = 2 ;
            c = 3 ;
        }
        rule h ( )
        {
            f ;
            g ;
        }
        h ;
        d = 4 ;
    """)
    run(t, """\
(b2db) break f
Breakpoint 1 set at f
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( ) at test.jam:7
7	            g ;
(b2db) next
8	            b = 2 ;
(b2db) next
9	            c = 3 ;
(b2db) next
14	            g ;
(b2db) next
17	        d = 4 ;
(b2db) quit
""")
    t.cleanup()

def test_next_breakpoint():
    """next should stop if it encounters a breakpoint.
    If the normal end point happens to be a breakpoint,
    then it should be reported as normal stepping."""
    t = make_tester()
    t.write("test.jam", """\
        rule f ( recurse ? )
        {
            if $(recurse) { f ; }
            a = 1 ;
        }
        rule g ( )
        {
            b = 2 ;
        }
        f true ;
        g ;
    """)
    run(t, """\
(b2db) break f
Breakpoint 1 set at f
(b2db) break g
Breakpoint 2 set at g
(b2db) break test.jam:4
Breakpoint 3 set at test.jam:4
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( true ) at test.jam:3
3	            if $(recurse) { f ; }
(b2db) next
Breakpoint 1, f ( ) at test.jam:3
3	            if $(recurse) { f ; }
(b2db) next
4	            a = 1 ;
(b2db) next
4	            a = 1 ;
(b2db) next
11	        g ;
(b2db) next
Breakpoint 2, g ( ) at test.jam:8
8	            b = 2 ;
(b2db) quit
""")
    t.cleanup()

def test_finish():
    t = make_tester()
    t.write("test.jam", """\
        rule f ( )
        {
            a = 1 ;
        }
        rule g ( )
        {
            f ;
            b = 2 ;
            i ;
        }
        rule h ( )
        {
            g ;
            i ;
        }
        rule i ( )
        {
            c = 3 ;
        }
        h ;
        d = 4 ;
    """)
    run(t, """\
(b2db) break f
Breakpoint 1 set at f
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( ) at test.jam:3
3	            a = 1 ;
(b2db) finish
8	            b = 2 ;
(b2db) finish
14	            i ;
(b2db) finish
21	        d = 4 ;
(b2db) quit
""")
    t.cleanup()

def test_finish_breakpoints():
    """finish should stop when it reaches a breakpoint."""
    t = make_tester()
    t.write("test.jam", """\
        rule f ( recurse * )
        {
            if $(recurse)
            {
                a = [ f $(recurse[2-]) ] ;
            }
        }
        rule g ( list * )
        {
            for local v in $(list)
            {
                x = $(v) ;
            }
        }
        f 1 2 ;
        g 1 2 ;
    """)
    run(t, """\
(b2db) break test.jam:5
Breakpoint 1 set at test.jam:5
(b2db) break test.jam:12
Breakpoint 2 set at test.jam:12
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( 1 2 ) at test.jam:5
5	                a = [ f $(recurse[2-]) ] ;
(b2db) finish
Breakpoint 1, f ( 2 ) at test.jam:5
5	                a = [ f $(recurse[2-]) ] ;
(b2db) finish
5	                a = [ f $(recurse[2-]) ] ;
(b2db) finish
16	        g 1 2 ;
(b2db) finish
Breakpoint 2, g ( 1 2 ) at test.jam:12
12	                x = $(v) ;
(b2db) finish
Breakpoint 2, g ( 1 2 ) at test.jam:12
12	                x = $(v) ;
(b2db) quit
""")
    t.cleanup()

def test_continue_breakpoints():
    """continue should stop when it reaches a breakpoint"""
    t = make_tester()
    t.write("test.jam", """\
        rule f ( recurse * )
        {
            if $(recurse)
            {
                a = [ f $(recurse[2-]) ] ;
            }
        }
        rule g ( list * )
        {
            for local v in $(list)
            {
                x = $(v) ;
            }
        }
        f 1 2 ;
        g 1 2 ;
    """)
    run(t, """\
(b2db) break test.jam:5
Breakpoint 1 set at test.jam:5
(b2db) break test.jam:12
Breakpoint 2 set at test.jam:12
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( 1 2 ) at test.jam:5
5	                a = [ f $(recurse[2-]) ] ;
(b2db) continue
Breakpoint 1, f ( 2 ) at test.jam:5
5	                a = [ f $(recurse[2-]) ] ;
(b2db) continue
Breakpoint 1, f ( 1 2 ) at test.jam:5
5	                a = [ f $(recurse[2-]) ] ;
(b2db) continue
Breakpoint 2, g ( 1 2 ) at test.jam:12
12	                x = $(v) ;
(b2db) continue
Breakpoint 2, g ( 1 2 ) at test.jam:12
12	                x = $(v) ;
(b2db) quit
""")
    t.cleanup()

def test_breakpoints():
    """Tests the interaction between the following commands:
    break, clear, delete, disable, enable"""
    t = make_tester()
    t.write("test.jam", """\
        rule f ( )
        {
            a = 1 ;
        }
        rule g ( )
        {
            b = 2 ;
        }
        rule h ( )
        {
            c = 3 ;
            d = 4 ;
        }
        f ;
        g ;
        h ;
        UPDATE ;
    """)
    run(t, """\
(b2db) break f
Breakpoint 1 set at f
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( ) at test.jam:3
3	            a = 1 ;
(b2db) kill
(b2db) break g
Breakpoint 2 set at g
(b2db) disable 1
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 2, g ( ) at test.jam:7
7	            b = 2 ;
(b2db) kill
(b2db) enable 1
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( ) at test.jam:3
3	            a = 1 ;
(b2db) kill
(b2db) delete 1
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 2, g ( ) at test.jam:7
7	            b = 2 ;
(b2db) kill
(b2db) break test.jam:12
Breakpoint 3 set at test.jam:12
(b2db) clear g
Deleted breakpoint 2
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 3, h ( ) at test.jam:12
12	            d = 4 ;
(b2db) kill
(b2db) clear test.jam:12
Deleted breakpoint 3
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Child {{\d+}} exited with status 0
(b2db) quit
""")
    t.cleanup()
    
def test_breakpoints_running():
    """Tests that breakpoints can be added and modified
    while the program is running."""
    t = make_tester()
    t.write("test.jam", """\
        rule f ( )
        {
            a = 1 ;
        }
        rule g ( )
        {
            b = 2 ;
        }
        rule h ( )
        {
            c = 3 ;
            d = 4 ;
        }
        f ;
        g ;
        h ;
        UPDATE ;
    """)
    run(t, """\
(b2db) break test.jam:14
Breakpoint 1 set at test.jam:14
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:14
14	        f ;
(b2db) break f
Breakpoint 2 set at f
(b2db) continue
Breakpoint 2, f ( ) at test.jam:3
3	            a = 1 ;
(b2db) kill
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:14
14	        f ;
(b2db) break g
Breakpoint 3 set at g
(b2db) disable 2
(b2db) continue
Breakpoint 3, g ( ) at test.jam:7
7	            b = 2 ;
(b2db) kill
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:14
14	        f ;
(b2db) enable 2
(b2db) continue
Breakpoint 2, f ( ) at test.jam:3
3	            a = 1 ;
(b2db) kill
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:14
14	        f ;
(b2db) delete 2
(b2db) continue
Breakpoint 3, g ( ) at test.jam:7
7	            b = 2 ;
(b2db) kill
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:14
14	        f ;
(b2db) break test.jam:12
Breakpoint 4 set at test.jam:12
(b2db) clear g
Deleted breakpoint 3
(b2db) continue
Breakpoint 4, h ( ) at test.jam:12
12	            d = 4 ;
(b2db) kill
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:14
14	        f ;
(b2db) clear test.jam:12
Deleted breakpoint 4
(b2db) continue
Child {{\d+}} exited with status 0
(b2db) quit
""")
    t.cleanup()

def test_backtrace():
    t = make_tester()
    t.write("test.jam", """\
        rule f ( x * : y * : z * )
        {
            return $(x) ;
        }
        rule g ( x * : y * : z * )
        {
            return [ f $(x) : $(y) : $(z) ] ;
        }
        g 1 : 2 : 3 ;
    """)
    run(t, """\
(b2db) break f
Breakpoint 1 set at f
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( 1 : 2 : 3 ) at test.jam:3
3	            return $(x) ;
(b2db) backtrace
#0  in f ( 1 : 2 : 3 ) at test.jam:3
#1  in g ( 1 : 2 : 3 ) at test.jam:7
#2  in module scope at test.jam:9
(b2db) quit
""")
    t.cleanup()

def test_print():
    t = make_tester()
    t.write("test.jam", """\
        rule f ( args * )
        {
            return $(args) ;
        }
        f x ;
        f x y ;
    """)

    run(t, """\
(b2db) break f
Breakpoint 1 set at f
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, f ( x ) at test.jam:3
3	            return $(args) ;
(b2db) print $(args)
x
(b2db) continue
Breakpoint 1, f ( x y ) at test.jam:3
3	            return $(args) ;
(b2db) print $(args)
x y
(b2db) disable 1
(b2db) print [ f z ]
z
(b2db) quit
""")

    t.cleanup()

def test_run_running():
    t = make_tester()
    t.write("test.jam", """\
        UPDATE ;
    """)

    run(t, """\
(b2db) break test.jam:1
Breakpoint 1 set at test.jam:1
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:1
1	        UPDATE ;
(b2db) run -ftest.jam
Child {{\d+}} exited with status 0
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:1
1	        UPDATE ;
(b2db) quit
""")

    t.cleanup()

def test_error_not_running():
    t = make_tester()
    run(t, """\
(b2db) continue
The program is not being run.
(b2db) step
The program is not being run.
(b2db) next
The program is not being run.
(b2db) finish
The program is not being run.
(b2db) kill
The program is not being run.
(b2db) backtrace
The program is not being run.
(b2db) print 1
The program is not being run.
(b2db) quit
""")

    t.cleanup()
    
def test_bad_arguments():
    t = make_tester()
    t.write("test.jam", """\
        UPDATE ;
    """)

    run(t, """\
(b2db) break test.jam:1
Breakpoint 1 set at test.jam:1
(b2db) run -ftest.jam
Starting program: {{bjam}} -ftest.jam
Breakpoint 1, module scope at test.jam:1
1	        UPDATE ;
(b2db) continue 1
Too many arguments to continue.
(b2db) step 1
Too many arguments to step.
(b2db) next 1
Too many arguments to next.
(b2db) finish 1
Too many arguments to finish.
(b2db) break
Missing argument to break.
(b2db) break x y
Too many arguments to break.
(b2db) disable
Missing argument to disable.
(b2db) disable 1 2
Too many arguments to disable.
(b2db) disable x
Invalid breakpoint number x.
(b2db) disable 2
Unknown breakpoint 2.
(b2db) enable
Missing argument to enable.
(b2db) enable 1 2
Too many arguments to enable.
(b2db) enable x
Invalid breakpoint number x.
(b2db) enable 2
Unknown breakpoint 2.
(b2db) delete
Missing argument to delete.
(b2db) delete 1 2
Too many arguments to delete.
(b2db) delete x
Invalid breakpoint number x.
(b2db) delete 2
Unknown breakpoint 2.
(b2db) clear
Missing argument to clear.
(b2db) clear test.jam:1 test.jam:1
Too many arguments to clear.
(b2db) clear test.jam:2
No breakpoint at test.jam:2.
(b2db) quit
""")

    t.cleanup()

def test_unknown_command():
    t = make_tester()
    run(t, """\
(b2db) xyzzy
Unknown command: xyzzy
(b2db) gnusto rezrov
Unknown command: gnusto
(b2db) quit
""")

    t.cleanup()

test_run()
test_exit_status()
test_step()
test_next()
test_next_breakpoint()
test_finish()
test_finish_breakpoints()
test_continue_breakpoints()
test_breakpoints()
test_breakpoints_running()
test_backtrace()
test_print()
test_run_running()
test_error_not_running()
test_bad_arguments()
test_unknown_command()
