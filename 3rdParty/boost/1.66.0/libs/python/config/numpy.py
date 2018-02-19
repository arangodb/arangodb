#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from . import ui
from contextlib import contextmanager

@contextmanager
def saved(context):
    save_cpppath = context.env.get('CPPPATH', [])
    save_libs = context.env.get('LIBS', [])
    yield context
    context.env.Replace(LIBS=save_libs)
    context.env.Replace(CPPPATH=save_cpppath)


def add_options(vars):

    pass


def check(context):

    numpy_source_file = r"""
// If defined, enforces linking againg PythonXXd.lib, which
// is usually not included in Python environments.
#undef _DEBUG
#include "Python.h"
#include "numpy/arrayobject.h"

#if PY_VERSION_HEX >= 0x03000000
void *initialize() { import_array();}
#else
void initialize() { import_array();}
#endif

int main()
{
  int result = 0;
  Py_Initialize();
  initialize();
  if (PyErr_Occurred())
  {
    result = 1;
  }
  else
  {
    npy_intp dims = 2;
    PyObject * a = PyArray_SimpleNew(1, &dims, NPY_INT);
    if (!a) result = 1;
    Py_DECREF(a);
  }
  Py_Finalize();
  return result;
}
"""

    import platform
    import subprocess
    import re, os

    def check_python(cmd):
        try:
            return True, subprocess.check_output([python, '-c', cmd]).strip()
        except subprocess.CalledProcessError as e:
            return False, e

    context.Message('Checking for NumPy...')
    with saved(context):
        python = context.env['PYTHON']
        result, numpy_incpath = check_python('import numpy; print(numpy.get_include())')
        if result:
            context.env.AppendUnique(CPPPATH=numpy_incpath)
            context.env.AppendUnique(LIBS=context.env['PYTHONLIBS'])
            result, output = context.TryRun(numpy_source_file,'.cpp')
        if not result:
            context.Result(0)
            return False
        context.env['NUMPY'] = True
        context.env['NUMPY_CPPPATH'] = numpy_incpath
        context.Result(1)
        return True
