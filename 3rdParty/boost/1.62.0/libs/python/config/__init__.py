#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from SCons.Variables import *
from SCons.Script import AddOption
from collections import OrderedDict
import platform
from . import ui
from . import cxx
from . import python
from . import boost

def add_options(vars):
    ui.add_option('-V', '--verbose', dest='verbose', action='store_true', help='verbose mode: print full commands.')
    python.add_options(vars)
    boost.add_options(vars)

    vars.Add('CXX')
    vars.Add('CPPPATH', converter=lambda v:v.split())
    vars.Add('CCFLAGS', converter=lambda v:v.split())
    vars.Add('CXXFLAGS', converter=lambda v:v.split())
    vars.Add('LIBPATH', converter=lambda v:v.split())
    vars.Add('LIBS', converter=lambda v:v.split())
    vars.Add('PYTHON')
    vars.Add('PYTHONLIBS')
    vars.Add('prefix')
    vars.Add('boostbook_prefix',
    vars.Add('CXX11'))

    ui.add_variable(vars, ("arch", "target architeture", platform.machine()))
    ui.add_variable(vars, ("toolchain", "toolchain to use", 'gcc'))
    ui.add_variable(vars, ListVariable("variant", "Build configuration", "release", ["release", "debug", "profile"]))
    ui.add_variable(vars, ListVariable("link", "Library linking", "dynamic", ["static", "dynamic"]))
    ui.add_variable(vars, ListVariable("threading", "Multi-threading support", "multi", ["single", "multi"]))
    ui.add_variable(vars, EnumVariable("layout", "Layout of library names and header locations", "versioned", ["versioned", "system"]))
    ui.add_variable(vars, PathVariable("stagedir", "If --stage is passed install only compiled library files in this location", "stage", PathVariable.PathAccept))
    ui.add_variable(vars, PathVariable("prefix", "Install prefix", "/usr/local", PathVariable.PathAccept))


def get_checks():
  checks = OrderedDict()
  checks['cxx'] = cxx.check
  checks['python'] = python.check
  checks['boost'] = boost.check
  return checks


def set_property(env, **kw):

    from toolchains.gcc import features as gcc_features
    from toolchains.msvc import features as msvc_features

    if 'gcc' in env['TOOLS']: features = gcc_features
    elif 'msvc' in env['TOOLS']: features = msvc_features
    else: raise Error('unknown toolchain')
    features.init_once(env)
    for (prop,value) in kw.items():
        getattr(features, prop, lambda x, y : None)(env, value)
        env[prop.upper()] = value


def boost_suffix(env):
    suffix = str()

    if env["layout"] == "versioned":
        if "gcc" in env["TOOLS"]:
            if env['CXX'] in ('clang', 'clang++'):
                suffix += "-clang" + "".join(env["CXXVERSION"].split(".")[0:2])
            else: # assume g++
                suffix += "-gcc" + "".join(env["CXXVERSION"].split(".")[0:2])
    if env["THREADING"] == "multi":
        suffix += "-mt"
    if env["DEBUG"]:
        suffix += "-d"
    if env["layout"] == "versioned":
        suffix += "-" + "_".join(env["BPL_VERSION"].split("."))

    return suffix


def prepare_build_dir(env):

    vars = {}
    env["boost_suffix"] = boost_suffix
    build_dir="bin.SCons"
    # FIXME: Support 'toolchain' variable properly.
    #        For now, we simply check whether $CXX refers to clang or gcc.
    if "gcc" in env["TOOLS"]:
        if env['CXX'] in ('clang', 'clang++'):
            build_dir+="/clang-%s"%env["CXXVERSION"]
        else: # assume g++
            build_dir+="/gcc-%s"%env["CXXVERSION"]
        default_cxxflags = ['-ftemplate-depth-128', '-Wall', '-g', '-O2']
        vars['CXXFLAGS'] = env.get('CXXFLAGS', default_cxxflags)
    elif "msvc" in env["TOOLS"]:
        build_dir+="/msvc-%s"%env["MSVS_VERSION"]
    vars['BOOST_BUILD_DIR'] = build_dir
    vars['BOOST_SUFFIX'] = "${boost_suffix(__env__)}"
    env.Replace(**vars)    
    return build_dir


def variants(env):

    env.Prepend(CPPPATH = "#/include", CPPDEFINES = ["BOOST_ALL_NO_LIB=1"])
    set_property(env, architecture = env['TARGET_ARCH'])
    for variant in env["variant"]:
        e = env.Clone()
        e["current_variant"] = variant
        set_property(env, profile = False)
        if variant == "release":
            set_property(e, optimize = "speed", debug = False)
        elif variant == "debug":
            set_property(e, optimize = "no", debug = True)
        elif variant == "profile":
            set_property(e, optimize = "speed", profile = True, debug = True)
        for linking in env["link"]:
            e["linking"] = linking
            if linking == "dynamic":
                e["LINK_DYNAMIC"] = True
            else:
                e["LINK_DYNAMIC"] = False
            for threading in e["threading"]:
                e["current_threading"] = threading
                set_property(e, threading = threading)
                yield e
