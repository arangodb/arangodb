#
# Copyright (c) 2016 Stefan Seefeld
# All rights reserved.
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from SCons.Script import AddOption, Flatten
from SCons.Script import Builder
from SCons.Action import Action
from SCons.Defaults import Copy
from SCons.Script import *
from subprocess import check_output, STDOUT, CalledProcessError
import sys
import os

def QuickBook(env, target, source, dependencies=[]):
    """Compile a QuickBook document to BoostBook."""

    for d in dependencies:
        env.Depends(target, d)
    env.Command(target, source, 'quickbook --input-file=$SOURCE --output-file=$TARGET')


def BoostBook(env, target, source, resources=[], args=[]):
    """Compile a BoostBook document to DocBook."""

    bb_prefix = env.GetOption('boostbook_prefix')
    stylesheet = bb_prefix + '/xsl/docbook.xsl'
    env.Command(target, source,
                'xsltproc {} -o $TARGET {} $SOURCE'.format(' '.join(args), stylesheet))


def BoostHTML(env, target, source, resources=[], args=[]):
    """Compile a DocBook document to HTML."""

    bb_prefix = env.GetOption('boostbook_prefix')
    stylesheet = bb_prefix + '/xsl/html.xsl'
    env.Command(target, source,
                'xsltproc {} -o $TARGET/ {} $SOURCE'.format(' '.join(args), stylesheet))
    prefix=Dir('.').path
    for r in resources:
        r = File(r).path[len(prefix)+1:]
        env.Depends(target, target + r)
        env.Command(target + r, r, Copy('$TARGET', '$SOURCE'))


def BoostRST(env, target, source, resources=[]):
    """Compile an RST document to HTML."""

    prefix=Dir('.').path
    for r in resources:
        r = File(r).path[len(prefix)+1:]
        env.Depends('html/' + r, r)
        env.Command('html/' + r, r, Copy('$TARGET', '$SOURCE'))
    env.Command(target, source,
                'rst2html --link-stylesheet --traceback --trim-footnote-reference-space --footnote-references=superscript --stylesheet=rst.css $SOURCE $TARGET')


def BoostSphinx(env, target, source):
    env.Sphinx(target, source)


def exists(env):
    return True


def generate(env):

    env.AddMethod(QuickBook)
    env.AddMethod(BoostBook)
    env.AddMethod(BoostHTML)
    env.AddMethod(BoostRST)
    env.AddMethod(BoostSphinx)
