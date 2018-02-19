# -*- python -*-
#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import SCons.Script.Main
import config
import config.ui
import platform
import os
import subprocess
import re

#
# We try to mimic the typical autotools-workflow.
#
# * In a 'configure' step all the essential build parameters are established
#   (either by explicit command-line arguments or from configure checks)
# * A subsequent build step can then simply read the cached variables, so
#   users don't have to memorize and re-issue the arguments on each subsequent
#   invocation, and neither do the config checks need to be re-run.
#
# The essential part here is to define a 'config' target, which removes any
# caches that may still be lingering around, then runs the checks.

if 'config' in COMMAND_LINE_TARGETS:
    # Clear the cache
    try: os.remove('bin.SCons/config.py')
    except: pass
if not os.path.exists('bin.SCons/'):
    os.mkdir('bin.SCons/')
vars = Variables('bin.SCons/config.py', ARGUMENTS)
config.add_options(vars)
arch = ARGUMENTS.get('arch', platform.machine())
env_vars = {}
if 'CXX' in os.environ: env_vars['CXX'] = os.environ['CXX']
if 'CXXFLAGS' in os.environ: env_vars['CXXFLAGS'] = os.environ['CXXFLAGS'].split()
env_vars['ENV'] = os.environ #{'PATH': os.environ['PATH'], 'TMP' : os.environ['TMP']}
env = Environment(toolpath=['config/tools'],
                  tools=['default', 'libs', 'tests', 'doc', 'sphinx4scons'],
                  variables=vars,
                  TARGET_ARCH=arch,
                  **env_vars)
if 'gcc' in env['TOOLS']:
    # Earlier SCons versions (~ 2.3.0) can't handle CXX=clang++.
    version = subprocess.check_output([env['CXX'], '--version'])
    match = re.search(r'[0-9]+(\.[0-9]+)+', version)
    if match:
        version = match.group(0)
    else:
        version = 'unknown'
    env['CXXVERSION'] = version

Help(config.ui.help(vars, env) + """
Variables are saved in bin.SCons/config.py and persist between scons invocations.
""")

if GetOption('help'):
    Return()

build_dir = config.prepare_build_dir(env)
config_log = '{}/config.log'.format(build_dir)

# configure
SConsignFile('{}/.sconsign'.format(build_dir))
#env.Decider('MD5-timestamp')
env.Decider('timestamp-newer')
checks = config.get_checks(env)
if 'config' in COMMAND_LINE_TARGETS:
    conf=env.Configure(custom_tests=checks, log_file=config_log, conf_dir=build_dir)
    if False in (getattr(conf, c)() for c in checks):
        Exit(1)
    env = conf.Finish()
    vars.Save('bin.SCons/config.py', env)

if not os.path.exists(config_log):
    print('Please run `scons config` first. (See `scons -h` for available options.)')
    Exit(1)

if not GetOption('verbose'):
    config.ui.pretty_output(env)    

# build
env['BPL_VERSION'] = '1.65'
for e in config.variants(env):
    variant_dir=e.subst("$BOOST_CURRENT_VARIANT_DIR")
    e.SConscript('src/SConscript', variant_dir=variant_dir + '/src',
                 exports = { 'env' : e.Clone(BOOST_LIB = 'python') })
    if 'test' in COMMAND_LINE_TARGETS:
        test_env = e.Clone(BOOST_LIB = 'python', BOOST_TEST = True)
        test_env.BoostUseLib('python')
        e.SConscript('test/SConscript', variant_dir=variant_dir + '/test',
                     exports = { 'env' : test_env })

if 'doc' in COMMAND_LINE_TARGETS:
    env.SConscript('doc/SConscript', variant_dir='bin.SCons/doc',
                   exports = { 'env' : e.Clone(BOOST_LIB = 'python') })
