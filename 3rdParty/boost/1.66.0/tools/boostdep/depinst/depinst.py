#!/usr/bin/env python

# depinst.py - installs the dependencies needed to test
#              a Boost library
#
# Copyright 2016 Peter Dimov
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt

import re
import sys
import os
import argparse

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

        print 'Cannot determine module for header', h

        return None

def scan_header_dependencies( f, x, gm, deps ):

    for line in f:

        m = re.match( '[ \t]*#[ \t]*include[ \t]*["<](boost/[^">]*)[">]', line )

        if m:

            h = m.group( 1 )

            mod = module_for_header( h, x, gm )

            if mod:

                if not mod in deps:

                    vprint( 'Adding dependency', mod )
                    deps[ mod ] = 0

def scan_directory( d, x, gm, deps ):

    vprint( 'Scanning directory', d )

    for root, dirs, files in os.walk( d ):

        for file in files:

            fn = os.path.join( root, file )

            vprint( 'Scanning file', fn )

            with open( fn, 'r' ) as f:

                scan_header_dependencies( f, x, gm, deps )

def scan_module_dependencies( m, x, gm, deps, dirs ):

    vprint( 'Scanning module', m )

    for dir in dirs:
        scan_directory( os.path.join( 'libs', m, dir ), x, gm, deps )

def read_exceptions():

    # exceptions.txt is the output of "boostdep --list-exceptions"

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

    gm = []

    with open( '.gitmodules', 'r' ) as f:

        for line in f:

            line = line.strip()
            
            m = re.match( 'path[ \t]*=[ \t]*(.*)$', line )

            if m:

                gm.append( m.group( 1 ) )
                
    return gm

def install_modules( deps, x, gm ):

    r = 0

    for m, i in deps.items():

        if not i:

            print 'Installing module', m
            os.system( 'git submodule -q update --init libs/' + m )

            r += 1

            deps[ m ] = 1 # mark as installed

            scan_module_dependencies( m, x, gm, deps, [ 'include', 'src' ] )

    return r

if( __name__ == "__main__" ):

    parser = argparse.ArgumentParser( description='Installs the dependencies needed to test a Boost library.' )

    parser.add_argument( '-v', '--verbose', help='enable verbose output', action='store_true' )
    parser.add_argument( '-I', '--include', help="additional subdirectory to scan; defaults are 'include', 'src', 'test'; can be repeated", metavar='DIR', action='append' )
    parser.add_argument( 'library', help="name of library to scan ('libs/' will be prepended)" )

    args = parser.parse_args()

    if args.verbose:

        def vprint( *args ):
            for arg in args:
                print arg,
            print

    else:

        def vprint( *args ):
            pass

    # vprint( '-I:', args.include )

    x = read_exceptions()
    # vprint( 'Exceptions:', x )

    gm = read_gitmodules()
    # vprint( '.gitmodules:', gm )

    m = args.library

    deps = { m : 1 }

    dirs = [ 'include', 'src', 'test' ]

    if args.include:
        for dir in args.include:
          dirs.append( dir )

    # vprint( 'Directories:', dirs )

    scan_module_dependencies( m, x, gm, deps, dirs )

    # vprint( 'Dependencies:', deps )

    while install_modules( deps, x, gm ):
        pass
