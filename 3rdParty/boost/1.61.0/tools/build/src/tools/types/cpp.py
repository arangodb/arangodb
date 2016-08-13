# Copyright David Abrahams 2004. Distributed under the Boost
# Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
import os
import re

import bjam

from b2.build import type as type_, scanner
from b2.manager import get_manager
from b2.util.utility import replace_grist


MANAGER = get_manager()
ENGINE = MANAGER.engine()
SCANNERS = MANAGER.scanners()


class CScanner(scanner.Scanner):
    def __init__(self, includes):
        scanner.Scanner.__init__(self)
        self.includes = []
        for include in includes:
            self.includes.extend(replace_grist(include, '').split('&&'))

    def pattern(self):
        return '#\s*include\s*(<(.*)>|"(.*)")'

    def process(self, target, matches, binding):
        # create a single string so that findall
        # can be used since it returns a list of
        # all grouped matches
        match_str = ' '.join(matches)
        # the question mark makes the regexes non-greedy
        angles = re.findall(r'<(.*?)>', match_str)
        quoted = re.findall(r'"(.*?)"', match_str)

        # CONSIDER: the new scoping rules seem to defeat "on target" variables.
        g = ENGINE.get_target_variable(target, 'HDRGRIST')
        b = os.path.normpath(os.path.dirname(binding))

        # Attach binding of including file to included targets. When a target is
        # directly created from a virtual target this extra information is
        # unnecessary. But in other cases, it allows us to distinguish between
        # two headers of the same name included from different places. We do not
        # need this extra information for angle includes, since they should not
        # depend on the including file (we can not get literal "." in the
        # include path).
        # local g2 = $(g)"#"$(b) ;
        g2 = g + '#' + b

        angles = [replace_grist(angle, g) for angle in angles]
        quoted = [replace_grist(quote, g2) for quote in quoted]

        includes = angles + quoted

        bjam.call('INCLUDES', target, includes)
        bjam.call('NOCARE', includes)
        ENGINE.set_target_variable(angles, 'SEARCH', self.includes)
        ENGINE.set_target_variable(quoted, 'SEARCH', [b] + self.includes)

        # Just propagate the current scanner to includes, in hope that includes
        # do not change scanners.
        SCANNERS.propagate(self, includes)

        bjam.call('ISFILE', includes)


scanner.register(CScanner, 'include')

type_.register_type('CPP', ['cpp', 'cxx', 'cc'])
type_.register_type('H', ['h'])
type_.register_type('HPP', ['hpp'], 'H')
type_.register_type('C', ['c'])
# It most cases where a CPP file or a H file is a source of some action, we
# should rebuild the result if any of files included by CPP/H are changed. One
# case when this is not needed is installation, which is handled specifically.
type_.set_scanner('CPP', CScanner)
type_.set_scanner('C', CScanner)
# One case where scanning of H/HPP files is necessary is PCH generation -- if
# any header included by HPP being precompiled changes, we need to recompile the
# header.
type_.set_scanner('H', CScanner)
type_.set_scanner('HPP', CScanner)
