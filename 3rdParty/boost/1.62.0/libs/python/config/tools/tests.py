#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from SCons.Script import AddOption, Flatten
from SCons.Script import Builder
from SCons.Action import Action
from subprocess import check_output, STDOUT, CalledProcessError
import sys
import os


def BoostCompileTest(env, test, source = None, **kw):

    def gen_result(target, source, env=env):
        target_file = target[0].abspath
        result_file = os.path.splitext(target_file)[0] + '.result'
        if sys.stdout.isatty():
            env['RESULT']='\033[92mPASS\033[0m'
        else:
            env['RESULT']='PASS'

        with open(result_file, 'w+') as result:
            result.write('Result: {}\n'.format('pass'))

    obj = env.Object(test, source if source is not None else test + '.cpp')
    env.AddPostAction(obj, Action(gen_result, cmdstr=None))
    env.AddPostAction(obj, Action('@echo $RESULT'))
    return obj

def BoostRun(env, prog, target, command = '$SOURCE'):

    def call(target, source, env=env):
        cmd = env.subst(command, target=target, source=source)
        result_file = env.subst('$TARGET', target=target)
        output=''
        try:
            output=check_output(cmd, stderr=STDOUT, shell=True, env=env['ENV'])
            success=True
        except CalledProcessError as e:
            output=e.output
            success=False
        with open(result_file, 'w+') as result:
            result.write('Result: {}\n'.format(success and 'pass' or 'fail'))
            result.write('Output: {}\n'.format(output))
        if sys.stdout.isatty():
            env['RESULT']=success and '\033[92mPASS\033[0m' or '\033[91mFAIL\033[0m'
        else:
            env['RESULT']=success and 'PASS' or 'FAIL'
     
    testcomstr = env.get('TESTCOMSTR')
    if testcomstr:
        run = env.Command(target, prog, Action(call, cmdstr=testcomstr))
    else:
        run = env.Command(target, prog, Action(call, cmdstr=command))
    env.AddPostAction(target, Action('@echo $RESULT'))
    return run


def BoostRunPythonScript(env, script):
    return env.BoostRun(env.File(script), script.replace('.py', '.result'), '"${PYTHON}" $SOURCE')


def BoostRunTest(env, test, source = None, command = '$SOURCE', command_sources = [], **kw):
    test_prog = env.Program(test, (source is None) and (test + ".cpp") or source, **kw)
    command += '> $TARGET'
    run = env.BoostRun([test_prog, command_sources], test + '.result', command)
    return run


def BoostRunTests(env, tests, **kw):
    run = []
    for test in Flatten(tests):
        run += env.BoostRunTest(test, **kw)
    return run

def BoostCompileTests(env, tests, **kw):
    comp = []
    for test in Flatten(tests):
        comp += env.BoostCompileTest(test, **kw)
    return comp


def BoostTestSummary(env, tests, **kw):

    def print_summary(target, source, **kw):
        results = tests
        failures = [r for r in results
                    if r.get_path().endswith('.result') and not 'Result: pass' in r.get_contents()]
        print('%s tests; %s pass; %s fails'%(len(results), len(results)-len(failures), len(failures)))
        if failures:
            print('For detailed failure reports, see:')
        for f in failures:
            print(f.get_path())

    testsumcomstr = env.get('TESTSUMCOMSTR')
    if testsumcomstr:
        run = env.Command('summary', tests, Action(print_summary, cmdstr=testsumcomstr))
    else:
        run = env.Command('summary', tests, print_summary, cmdstr='')

    



def exists(env):
    return True


def generate(env):
    AddOption('--test', dest='test', action="store_true")

    env.AddMethod(BoostCompileTest)
    env.AddMethod(BoostRun)
    env.AddMethod(BoostRunPythonScript)
    env.AddMethod(BoostRunTest)
    env.AddMethod(BoostRunTests)
    env.AddMethod(BoostCompileTests)
    env.AddMethod(BoostTestSummary)
