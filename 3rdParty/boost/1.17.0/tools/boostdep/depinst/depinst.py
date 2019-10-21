#!/usr/bin/env python

# depinst.py - installs the dependencies needed to test
#              a Boost library
#
# Copyright 2016-2019 Peter Dimov
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt

from __future__ import print_function

import re
import sys
import os
import argparse

verbose = 0

def vprint( level, *args ):

    if verbose >= level:
        print( *args )


def is_module( m, gm ):

    return ( 'libs/' + m ) in gm


def module_for_header( h, x, gm ):

    if h in x:

        return x[ h ]

    else:

        # boost/function.hpp
        m = re.match( 'boost/([^\\./]*)\\.h[a-z]*$', h )

        if m and is_module( m.group( 1 ), gm ):

            return m.group( 1 )

        # boost/numeric/conversion.hpp
        m = re.match( 'boost/([^/]*/[^\\./]*)\\.h[a-z]*$', h )

        if m and is_module( m.group( 1 ), gm ):

            return m.group( 1 )

        # boost/numeric/conversion/header.hpp
        m = re.match( 'boost/([^/]*/[^/]*)/', h )

        if m and is_module( m.group( 1 ), gm ):

            return m.group( 1 )

        # boost/function/header.hpp
        m = re.match( 'boost/([^/]*)/', h )

        if m and is_module( m.group( 1 ), gm ):

            return m.group( 1 )

        vprint( 0, 'Cannot determine module for header', h )

        return None


def scan_header_dependencies( f, x, gm, deps ):

    for line in f:

        m = re.match( '[ \t]*#[ \t]*include[ \t]*["<](boost/[^">]*)[">]', line )

        if m:

            h = m.group( 1 )

            mod = module_for_header( h, x, gm )

            if mod:

                if not mod in deps:

                    vprint( 1, 'Adding dependency', mod )
                    deps[ mod ] = 0


def scan_directory( d, x, gm, deps ):

    vprint( 1, 'Scanning directory', d )

    if os.name == 'nt' and sys.version_info[0] < 3:
        d = unicode( d )

    for root, dirs, files in os.walk( d ):

        for file in files:

            fn = os.path.join( root, file )

            vprint( 2, 'Scanning file', fn )

            if sys.version_info[0] < 3:

                with open( fn, 'r' ) as f:
                    scan_header_dependencies( f, x, gm, deps )

            else:

                with open( fn, 'r', encoding='latin-1' ) as f:
                    scan_header_dependencies( f, x, gm, deps )


def scan_module_dependencies( m, x, gm, deps, dirs ):

    vprint( 1, 'Scanning module', m )

    for dir in dirs:
        scan_directory( os.path.join( 'libs', m, dir ), x, gm, deps )


def read_exceptions():

    # exceptions.txt is the output of "boostdep --list-exceptions"

    vprint( 1, 'Reading exceptions.txt' )

    x = {}

    module = None

    with open( os.path.join( os.path.dirname( sys.argv[0] ), 'exceptions.txt' ), 'r' ) as f:

        for line in f:

            line = line.rstrip()

            m = re.match( '(.*):$', line )

            if m:

                module = m.group( 1 ).replace( '~', '/' )

            else:

                header = line.lstrip()
                x[ header ] = module

    return x


def read_gitmodules():

    vprint( 1, 'Reading .gitmodules' )

    gm = []

    with open( '.gitmodules', 'r' ) as f:

        for line in f:

            line = line.strip()

            m = re.match( 'path[ \t]*=[ \t]*(.*)$', line )

            if m:

                gm.append( m.group( 1 ) )

    return gm

def install_modules( modules, git_args ):

    if len( modules ) == 0:
        return

    vprint( 0, 'Installing:', ', '.join(modules) )

    modules = [ 'libs/' + m for m in modules ]

    command = 'git submodule'

    if verbose <= 0:
        command += ' -q'

    command += ' update --init ' + git_args + ' ' + ' '.join( modules )

    vprint( 1, 'Executing:', command )
    os.system( command );


def install_module_dependencies( deps, x, gm, git_args ):

    modules = []

    for m, i in deps.items():

        if not i:

            modules += [ m ]
            deps[ m ] = 1 # mark as installed

    if len( modules ) == 0:
        return 0

    install_modules( modules, git_args )

    for m in modules:
        scan_module_dependencies( m, x, gm, deps, [ 'include', 'src' ] )

    return len( modules )


if( __name__ == "__main__" ):

    parser = argparse.ArgumentParser( description='Installs the dependencies needed to test a Boost library.' )

    parser.add_argument( '-v', '--verbose', help='enable verbose output', action='count', default=0 )
    parser.add_argument( '-q', '--quiet', help='quiet output (opposite of -v)', action='count', default=0 )
    parser.add_argument( '-X', '--exclude', help="exclude a default subdirectory ('include', 'src', or 'test') from scan; can be repeated", metavar='DIR', action='append', default=[] )
    parser.add_argument( '-N', '--ignore', help="exclude top-level dependency even when found in scan; can be repeated", metavar='LIB', action='append', default=[] )
    parser.add_argument( '-I', '--include', help="additional subdirectory to scan; can be repeated", metavar='DIR', action='append', default=[] )
    parser.add_argument( '-g', '--git_args', help="additional arguments to `git submodule update`", default='', action='store' )
    parser.add_argument( 'library', help="name of library to scan ('libs/' will be prepended)" )

    args = parser.parse_args()

    verbose = args.verbose - args.quiet

    vprint( 2, '-X:', args.exclude )
    vprint( 2, '-I:', args.include )
    vprint( 2, '-N:', args.ignore )

    x = read_exceptions()
    vprint( 2, 'Exceptions:', x )

    gm = read_gitmodules()
    vprint( 2, '.gitmodules:', gm )

    essentials = [ 'config', 'headers', '../tools/boost_install', '../tools/build' ]

    essentials = [ e for e in essentials if os.path.exists( 'libs/' + e ) ]

    install_modules( essentials, args.git_args )

    m = args.library

    deps = { m : 1 }

    dirs = [ 'include', 'src', 'test' ]

    for dir in args.exclude:
      dirs.remove( dir )

    for dir in args.include:
      dirs.append( dir )

    vprint( 1, 'Directories to scan:', *dirs )

    scan_module_dependencies( m, x, gm, deps, dirs )

    for dep in args.ignore:
        if dep in deps:
            vprint( 1, 'Ignoring dependency', dep )
            del deps[dep]

    vprint( 2, 'Dependencies:', deps )

    while install_module_dependencies( deps, x, gm, args.git_args ):
        pass
