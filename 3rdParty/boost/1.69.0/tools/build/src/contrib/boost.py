# $Id: boost.jam 62249 2010-05-26 19:05:19Z steven_watanabe $
# Copyright 2008 Roland Schwarz
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

# Boost library support module.
#
# This module allows to use the boost library from boost-build projects.
# The location of a boost source tree or the path to a pre-built
# version of the library can be configured from either site-config.jam
# or user-config.jam. If no location  is configured the module looks for
# a BOOST_ROOT environment variable, which should point to a boost source
# tree. As a last resort it tries to use pre-built libraries from the standard
# search path of the compiler.
#
# If the location to a source tree is known, the module can be configured
# from the *-config.jam files:
#
# using boost : 1.35 : <root>/path-to-boost-root ;
#
# If the location to a pre-built version is known:
#
# using boost : 1.34
#   : <include>/usr/local/include/boost_1_34
#     <library>/usr/local/lib
#   ;
#
# It is legal to configure more than one boost library version in the config
# files. The version identifier is used to disambiguate between them.
# The first configured version becomes the default.
#
# To use a boost library you need to put a 'use' statement into your
# Jamfile:
#
# import boost ;
#
# boost.use-project 1.35 ;
#
# If you don't care about a specific version you just can omit the version
# part, in which case the default is picked up:
#
# boost.use-project ;
#
# The library can be referenced with the project identifier '/boost'. To
# reference the program_options you would specify:
#
# exe myexe : mysrc.cpp : <library>/boost//program_options ;
#
# Note that the requirements are automatically transformed into suitable
# tags to find the correct pre-built library.
#

import re

import bjam

from b2.build import alias, property, property_set, feature
from b2.manager import get_manager
from b2.tools import builtin, common
from b2.util import bjam_signature, regex


# TODO: This is currently necessary in Python Port, but was not in Jam.
feature.feature('layout', ['system', 'versioned', 'tag'], ['optional'])
feature.feature('root', [], ['optional', 'free'])
feature.feature('build-id', [], ['optional', 'free'])

__initialized = None
__boost_auto_config = property_set.create([property.Property('layout', 'system')])
__boost_configured = {}
__boost_default = None
__build_id = None

__debug = None

def debug():
    global __debug
    if __debug is None:
        __debug = "--debug-configuration" in bjam.variable("ARGV")
    return __debug


# Configuration of the boost library to use.
#
# This can either be a boost source tree or
# pre-built libraries. The 'version' parameter must be a valid boost
# version number, e.g. 1.35, if specifying a pre-built version with
# versioned layout. It may be a symbolic name, e.g. 'trunk' if specifying
# a source tree. The options are specified as named parameters (like
# properties). The following parameters are available:
#
# <root>/path-to-boost-root: Specify a source tree.
#
# <include>/path-to-include: The include directory to search.
#
# <library>/path-to-library: The library directory to search.
#
# <layout>system or <layout>versioned.
#
# <build-id>my_build_id: The custom build id to use.
#
def init(version, options = None):
    assert(isinstance(version,list))
    assert(len(version)==1)
    version = version[0]
    if version in __boost_configured:
        get_manager().errors()("Boost {} already configured.".format(version));
    else:
        global __boost_default
        if debug():
            if not __boost_default:
                print "notice: configuring default boost library {}".format(version)
            print "notice: configuring boost library {}".format(version)

        if not __boost_default:
            __boost_default = version
        properties = []
        for option in options:
            properties.append(property.create_from_string(option))
        __boost_configured[ version ] = property_set.PropertySet(properties)

projects = get_manager().projects()
rules = projects.project_rules()


# Use a certain version of the library.
#
# The use-project rule causes the module to define a boost project of
# searchable pre-built boost libraries, or references a source tree
# of the boost library. If the 'version' parameter is omitted either
# the configured default (first in config files) is used or an auto
# configuration will be attempted.
#
@bjam_signature(([ "version", "?" ], ))
def use_project(version = None):
    projects.push_current( projects.current() )
    if not version:
        version = __boost_default
    if not version:
        version = "auto_config"

    global __initialized
    if __initialized:
        if __initialized != version:
            get_manager().errors()('Attempt to use {} with different parameters'.format('boost'))
    else:
        if version in __boost_configured:
            opts = __boost_configured[ version ]
            root = opts.get('<root>' )
            inc = opts.get('<include>')
            lib = opts.get('<library>')

            if debug():
                print "notice: using boost library {} {}".format( version, opt.raw() )

            global __layout
            global __version_tag
            __layout = opts.get('<layout>')
            if not __layout:
                __layout = 'versioned'
            __build_id = opts.get('<build-id>')
            __version_tag = re.sub("[*\\/:.\"\' ]", "_", version)
            __initialized = version

            if ( root and inc ) or \
                ( root and lib ) or \
                ( lib and not inc ) or \
                ( not lib and inc ):
                get_manager().errors()("Ambiguous parameters, use either <root> or <include> with <library>.")
            elif not root and not inc:
                root = bjam.variable("BOOST_ROOT")

            module = projects.current().project_module()

            if root:
                bjam.call('call-in-module', module, 'use-project', ['boost', root])
            else:
                projects.initialize(__name__)
                if version == '0.0.1':
                    boost_0_0_1( inc, lib )
                else:
                    boost_std( inc, lib )
        else:
            get_manager().errors()("Reference to unconfigured boost version.")
    projects.pop_current()


rules.add_rule( 'boost.use-project', use_project )

def boost_std(inc = None, lib = None):
    # The default definitions for pre-built libraries.
    rules.project(
        ['boost'],
        ['usage-requirements'] + ['<include>{}'.format(i) for i in inc] + ['<define>BOOST_ALL_NO_LIB'],
        ['requirements'] + ['<search>{}'.format(l) for l in lib])

    # TODO: There should be a better way to add a Python function into a
    # project requirements property set.
    tag_prop_set = property_set.create([property.Property('<tag>', tag_std)])
    attributes = projects.attributes(projects.current().project_module())
    attributes.requirements = attributes.requirements.refine(tag_prop_set)

    alias('headers')

    def boost_lib(lib_name, dyn_link_macro):
        if (isinstance(lib_name,str)):
            lib_name = [lib_name]
        builtin.lib(lib_name, usage_requirements=['<link>shared:<define>{}'.format(dyn_link_macro)])

    boost_lib('container'           , 'BOOST_CONTAINER_DYN_LINK'      )
    boost_lib('date_time'           , 'BOOST_DATE_TIME_DYN_LINK'      )
    boost_lib('filesystem'          , 'BOOST_FILE_SYSTEM_DYN_LINK'    )
    boost_lib('graph'               , 'BOOST_GRAPH_DYN_LINK'          )
    boost_lib('graph_parallel'      , 'BOOST_GRAPH_DYN_LINK'          )
    boost_lib('iostreams'           , 'BOOST_IOSTREAMS_DYN_LINK'      )
    boost_lib('locale'              , 'BOOST_LOG_DYN_LINK'            )
    boost_lib('log'                 , 'BOOST_LOG_DYN_LINK'            )
    boost_lib('log_setup'           , 'BOOST_LOG_DYN_LINK'            )
    boost_lib('math_tr1'            , 'BOOST_MATH_TR1_DYN_LINK'       )
    boost_lib('math_tr1f'           , 'BOOST_MATH_TR1_DYN_LINK'       )
    boost_lib('math_tr1l'           , 'BOOST_MATH_TR1_DYN_LINK'       )
    boost_lib('math_c99'            , 'BOOST_MATH_TR1_DYN_LINK'       )
    boost_lib('math_c99f'           , 'BOOST_MATH_TR1_DYN_LINK'       )
    boost_lib('math_c99l'           , 'BOOST_MATH_TR1_DYN_LINK'       )
    boost_lib('mpi'                 , 'BOOST_MPI_DYN_LINK'            )
    boost_lib('program_options'     , 'BOOST_PROGRAM_OPTIONS_DYN_LINK')
    boost_lib('python'              , 'BOOST_PYTHON_DYN_LINK'         )
    boost_lib('python3'             , 'BOOST_PYTHON_DYN_LINK'         )
    boost_lib('random'              , 'BOOST_RANDOM_DYN_LINK'         )
    boost_lib('regex'               , 'BOOST_REGEX_DYN_LINK'          )
    boost_lib('serialization'       , 'BOOST_SERIALIZATION_DYN_LINK'  )
    boost_lib('wserialization'      , 'BOOST_SERIALIZATION_DYN_LINK'  )
    boost_lib('signals'             , 'BOOST_SIGNALS_DYN_LINK'        )
    boost_lib('system'              , 'BOOST_SYSTEM_DYN_LINK'         )
    boost_lib('unit_test_framework' , 'BOOST_TEST_DYN_LINK'           )
    boost_lib('prg_exec_monitor'    , 'BOOST_TEST_DYN_LINK'           )
    boost_lib('test_exec_monitor'   , 'BOOST_TEST_DYN_LINK'           )
    boost_lib('thread'              , 'BOOST_THREAD_DYN_DLL'          )
    boost_lib('wave'                , 'BOOST_WAVE_DYN_LINK'           )

def boost_0_0_1( inc, lib ):
    print "You are trying to use an example placeholder for boost libs." ;
    # Copy this template to another place (in the file boost.jam)
    # and define a project and libraries modelled after the
    # boost_std rule. Please note that it is also possible to have
    # a per version taging rule in case they are different between
    # versions.

def tag_std(name, type, prop_set):
    name = 'boost_' + name
    if 'static' in prop_set.get('<link>') and 'windows' in prop_set.get('<target-os>'):
        name = 'lib' + name
    result = None

    if __layout == 'system':
        versionRe = re.search('^([0-9]+)_([0-9]+)', __version_tag)
        if versionRe and versionRe.group(1) == '1' and int(versionRe.group(2)) < 39:
            result = tag_tagged(name, type, prop_set)
        else:
            result = tag_system(name, type, prop_set)
    elif __layout == 'tagged':
        result = tag_tagged(name, type, prop_set)
    elif __layout == 'versioned':
        result = tag_versioned(name, type, prop_set)
    else:
        get_manager().errors()("Missing layout")
    return result

def tag_maybe(param):
    return ['-{}'.format(param)] if param else []

def tag_system(name, type, prop_set):
    return common.format_name(['<base>'] + tag_maybe(__build_id), name, type, prop_set)

def tag_tagged(name, type, prop_set):
    return common.format_name(['<base>', '<threading>', '<runtime>'] + tag_maybe(__build_id), name, type, prop_set)

def tag_versioned(name, type, prop_set):
    return common.format_name(['<base>', '<toolset>', '<threading>', '<runtime>'] + tag_maybe(__version_tag) + tag_maybe(__build_id),
        name, type, prop_set)
