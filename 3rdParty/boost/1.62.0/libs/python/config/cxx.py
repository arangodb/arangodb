#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from . import ui
import os

def add_options(vars):

    pass

def check(context):

    source = r"""#if __cplusplus < 201103L
#error no C++11
#endif"""

    context.Message('Checking for C++11 support...')

    if not context.TryCompile(source, '.cpp'):
        context.env['CXX11'] = False
        context.Result(0)
    else:
        context.env['CXX11'] = True
        context.Result(1)
    return True
