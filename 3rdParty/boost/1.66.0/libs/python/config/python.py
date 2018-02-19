#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from . import ui

def add_options(vars):

    ui.add_option('--python', help='the python executable')


def check(context):

    python_source_file = r"""
// If defined, enforces linking againg PythonXXd.lib, which
// is usually not included in Python environments.
#undef _DEBUG
#include "Python.h"
int main()
{
  Py_Initialize();
  Py_Finalize();
  return 0;
}
"""

    import platform
    import subprocess
    import re, os

    def check_python(cmd):
        return subprocess.check_output([python, '-c', cmd]).strip()

    def check_sysconfig(cmd):
        r = check_python('import distutils.sysconfig as c; print(c.%s)'%cmd)
        return r if r != 'None' else ''

    context.Message('Checking for Python...')
    python = context.env.GetOption('python') or 'python'
    context.env['PYTHON'] = python
    incpath = check_sysconfig('get_python_inc()')
    context.env.AppendUnique(CPPPATH=[incpath])
    if platform.system() == 'Windows':
        version = check_python('import sys; print("%d%d"%sys.version_info[0:2])')
        prefix = check_python('import sys; print(sys.prefix)')
        libfile = os.path.join(prefix, 'libs', 'python%s.lib'%version)
        libpath = os.path.join(prefix, 'libs')
        lib = 'python%s'%version
        context.env.AppendUnique(LIBS=[lib])
    else:
        libpath = check_sysconfig('get_config_var("LIBDIR")')
        libfile = check_sysconfig('get_config_var("LIBRARY")')
        match = re.search('(python.*)\.(a|so|dylib)', libfile)
        lib = None
        if match:
            lib = match.group(1)
            context.env.AppendUnique(PYTHONLIBS=[lib])
            if match.group(2) == 'a':
                flags = check_sysconfig('get_config_var("LINKFORSHARED")')
                if flags is not None:
                    context.env.AppendUnique(LINKFLAGS=flags.split())
    context.env.AppendUnique(LIBPATH=[libpath])
    oldlibs = context.AppendLIBS([lib])
    flags = check_sysconfig('get_config_var("MODLIBS")')
    flags += ' ' + check_sysconfig('get_config_var("SHLIBS")')
    flags = [f[2:] for f in flags.strip().split() if f.startswith('-l')]
    if flags:
        context.AppendLIBS([flags])
    result = context.TryLink(python_source_file,'.cpp')
    if not result and context.env['PLATFORM'] == 'darwin':
        # Sometimes we need some extra stuff on Mac OS
        frameworkDir = libpath # search up the libDir tree for the proper home for frameworks
        while frameworkDir and frameworkDir != "/":
            frameworkDir, d2 = os.path.split(frameworkDir)
            if d2 == "Python.framework":
                if not "Python" in os.listdir(os.path.join(frameworkDir, d2)):
                    context.Result(0)
                    print((
                        "Expected to find Python in framework directory %s, but it isn't there"
                        % frameworkDir))
                    return False
                break
        context.env.AppendUnique(LINKFLAGS="-F%s" % frameworkDir)
        result = context.TryLink(python_source_file,'.cpp')
    if not result:
        context.Result(0)
        print("Cannot link program with Python.")
        return False
    if context.env['PLATFORM'] == 'darwin':
        context.env['LDMODULESUFFIX'] = '.so'
    context.Result(1)
    context.SetLIBS(oldlibs)
    context.env.AppendUnique(PYTHONLIBS=[lib] + flags)
    return True
