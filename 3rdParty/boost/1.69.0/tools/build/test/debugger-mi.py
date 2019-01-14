#!/usr/bin/python

# Copyright 2016 Steven Watanabe
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Test the mi interface for the debugger

import BoostBuild
import TestCmd
import re

def split_stdin_stdout(text):
    """stdin is all text after the prompt up to and including
    the next newline.  Everything else is stdout.  stdout
    may contain regular expressions enclosed in {{}}."""
    prompt = re.escape('(gdb) \n')
    pattern = re.compile('(?<=%s)((?:\d*-.*)\n)' % prompt)
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
    return BoostBuild.Tester(["-dmi"], pass_toolset=False, pass_d0=False,
        use_test_config=False, ignore_toolset_requirements=False, match=TestCmd.match_re)

def test_exec_run():
    t = make_tester()
    t.write("test.jam", """\
        UPDATE ;
    """)

    run(t, """\
=thread-group-added,id="i1"
(gdb) 
72-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
72^running
(gdb) 
*stopped,reason="exited-normally"
(gdb) 
73-gdb-exit
73^exit
""")

    t.cleanup()

def test_exit_status():
    t = make_tester()
    t.write("test.jam", """\
        EXIT : 1 ;
    """)
    run(t, """\
=thread-group-added,id="i1"
(gdb) 
72-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
72^running
(gdb) 

*stopped,reason="exited",exit-code="1"
(gdb) 
73-gdb-exit
73^exit
""")
    t.cleanup()

def test_exec_step():
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
=thread-group-added,id="i1"
(gdb) 
-break-insert f
^done,bkpt={number="1",type="breakpoint",disp="keep",enabled="y",func="f"}
(gdb) 
72-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
72^running
(gdb) 
*stopped,reason="breakpoint-hit",bkptno="1",disp="keep",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="8"},thread-id="1",stopped-threads="all"
(gdb) 
1-exec-step
1^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="g",args=[],file="test.jam",fullname="{{.*}}test.jam",line="3"},thread-id="1"
(gdb) 
2-exec-step
2^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="g",args=[],file="test.jam",fullname="{{.*}}test.jam",line="4"},thread-id="1"
(gdb) 
3-exec-step
3^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="9"},thread-id="1"
(gdb) 
73-gdb-exit
73^exit
""")
    t.cleanup()
    
def test_exec_next():
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
=thread-group-added,id="i1"
(gdb) 
-break-insert f
^done,bkpt={number="1",type="breakpoint",disp="keep",enabled="y",func="f"}
(gdb) 
72-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
72^running
(gdb) 
*stopped,reason="breakpoint-hit",bkptno="1",disp="keep",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="7"},thread-id="1",stopped-threads="all"
(gdb) 
1-exec-next
1^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="8"},thread-id="1"
(gdb) 
2-exec-next
2^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="9"},thread-id="1"
(gdb) 
3-exec-next
3^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="h",args=[],file="test.jam",fullname="{{.*}}test.jam",line="14"},thread-id="1"
(gdb) 
4-exec-next
4^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="module scope",args=[],file="test.jam",fullname="{{.*}}test.jam",line="17"},thread-id="1"
(gdb) 
73-gdb-exit
73^exit
""")
    t.cleanup()

def test_exec_finish():
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
=thread-group-added,id="i1"
(gdb) 
-break-insert f
^done,bkpt={number="1",type="breakpoint",disp="keep",enabled="y",func="f"}
(gdb) 
72-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
72^running
(gdb) 
*stopped,reason="breakpoint-hit",bkptno="1",disp="keep",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="3"},thread-id="1",stopped-threads="all"
(gdb) 
1-exec-finish
1^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="g",args=[],file="test.jam",fullname="{{.*}}test.jam",line="8"},thread-id="1"
(gdb) 
2-exec-finish
2^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="h",args=[],file="test.jam",fullname="{{.*}}test.jam",line="14"},thread-id="1"
(gdb) 
3-exec-finish
3^running
(gdb) 
*stopped,reason="end-stepping-range",frame={func="module scope",args=[],file="test.jam",fullname="{{.*}}test.jam",line="21"},thread-id="1"
(gdb) 
73-gdb-exit
73^exit
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
=thread-group-added,id="i1"
(gdb) 
-break-insert f
^done,bkpt={number="1",type="breakpoint",disp="keep",enabled="y",func="f"}
(gdb) 
72-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
72^running
(gdb) 
*stopped,reason="breakpoint-hit",bkptno="1",disp="keep",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="3"},thread-id="1",stopped-threads="all"
(gdb) 
-interpreter-exec console kill
^done
(gdb) 
-break-insert g
^done,bkpt={number="2",type="breakpoint",disp="keep",enabled="y",func="g"}
(gdb) 
-break-disable 1
^done
(gdb) 
73-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
73^running
(gdb) 
*stopped,reason="breakpoint-hit",bkptno="2",disp="keep",frame={func="g",args=[],file="test.jam",fullname="{{.*}}test.jam",line="7"},thread-id="1",stopped-threads="all"
(gdb) 
-interpreter-exec console kill
^done
(gdb) 
-break-enable 1
^done
(gdb) 
74-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
74^running
(gdb) 
*stopped,reason="breakpoint-hit",bkptno="1",disp="keep",frame={func="f",args=[],file="test.jam",fullname="{{.*}}test.jam",line="3"},thread-id="1",stopped-threads="all"
(gdb) 
-interpreter-exec console kill
^done
(gdb) 
-break-delete 1
^done
(gdb) 
75-exec-run -ftest.jam
=thread-created,id="1",group-id="i1"
75^running
(gdb) 
*stopped,reason="breakpoint-hit",bkptno="2",disp="keep",frame={func="g",args=[],file="test.jam",fullname="{{.*}}test.jam",line="7"},thread-id="1",stopped-threads="all"
(gdb) 
76-gdb-exit
76^exit
""")
    t.cleanup()

test_exec_run()
test_exit_status()
test_exec_step()
test_exec_next()
test_exec_finish()
test_breakpoints()
