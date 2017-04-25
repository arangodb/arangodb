#!/usr/bin/env python

from itertools import combinations

def powerset(items):
    result = []
    for i in xrange(len(items) + 1):
        result += combinations(items, i)
    return result

MAKE_J_VAL = 32

possible_compilers = [('gcc', 'g++'), ('clang', 'clang++')]
possible_compiler_opts = [
    '-m32',
]
possible_config_opts = [
    '--enable-debug',
    '--enable-prof',
    '--disable-stats',
    '--disable-tcache',
]

print 'set -e'
print 'autoconf'
print 'unamestr=`uname`'

for cc, cxx in possible_compilers:
    for compiler_opts in powerset(possible_compiler_opts):
        for config_opts in powerset(possible_config_opts):
            config_line = (
                './configure '
                + 'CC="{} {}" '.format(cc, " ".join(compiler_opts))
                + 'CXX="{} {}" '.format(cxx, " ".join(compiler_opts))
                + " ".join(config_opts)
            )
            # Heap profiling is not supported on OS X.
            if '--enable-prof' in config_opts:
                print 'if [[ "$unamestr" != "Darwin" ]]; then'
            print config_line
            print "make clean"
            print "make -j" + str(MAKE_J_VAL) + " check"
            if '--enable-prof' in config_opts:
                print 'fi'
