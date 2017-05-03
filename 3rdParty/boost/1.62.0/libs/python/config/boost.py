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
    
    ui.add_option("--boost-prefix", dest="boost_prefix", type="string", nargs=1, action="store",
                  metavar="DIR", default=os.environ.get("BOOST_DIR"),
                  help="prefix for Boost libraries; should have 'include' and 'lib' subdirectories, 'boost' and 'stage\\lib' subdirectories on Windows")
    ui.add_option("--boost-include", dest="boost_include", type="string", nargs=1, action="store",
                  metavar="DIR", help="location of Boost header files")
    ui.add_option("--boostbook-prefix", dest="boostbook_prefix", type="string",
                  nargs=1, action="store",
                  metavar="DIR", default="/usr/share/boostbook",
                  help="prefix for BoostBook stylesheets")
    
def check(context):

    boost_source_file = r"#include <boost/config.hpp>"

    context.Message('Checking for Boost...')

    boost_prefix = context.env.GetOption('boost_prefix')
    boost_include = context.env.GetOption('boost_include')
    boostbook_prefix = context.env.GetOption('boostbook_prefix')
    incpath=None
    if boost_include:
        incpath=boost_include
    elif boost_prefix:
        incpath=boost_prefix
    if incpath:
        context.env.AppendUnique(CPPPATH=[incpath])
    if not context.TryCompile(boost_source_file, '.cpp'):
        context.Result(0)
        return False
    context.env.AppendUnique(boostbook_prefix=boostbook_prefix)
    context.Result(1)
    return True
