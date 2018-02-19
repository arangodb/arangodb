#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# Based on SCons/Tool/gcc.py

import os
import re
import subprocess

import SCons.Util
import SCons.Tool.cc

compilers = ['clang']

def generate(env):
    """Add Builders and construction variables for clang to an Environment."""
    SCons.Tool.cc.generate(env)

    env['CC'] = env.Detect(compilers) or 'clang'
    if env['PLATFORM'] in ['cygwin', 'win32']:
        env['SHCCFLAGS'] = SCons.Util.CLVar('$CCFLAGS')
    else:
        env['SHCCFLAGS'] = SCons.Util.CLVar('$CCFLAGS -fPIC')
    # determine compiler version
    if env['CC']:
        #pipe = SCons.Action._subproc(env, [env['CC'], '-dumpversion'],
        pipe = SCons.Action._subproc(env, [env['CC'], '--version'],
                                     stdin = 'devnull',
                                     stderr = 'devnull',
                                     stdout = subprocess.PIPE)
        if pipe.wait() != 0: return
        # clang -dumpversion is of no use
        line = pipe.stdout.readline()
        match = re.search(r'clang +version +([0-9]+(?:\.[0-9]+)+)', line)
        if match:
            env['CCVERSION'] = match.group(1)

def exists(env):
    return env.Detect(compilers)
