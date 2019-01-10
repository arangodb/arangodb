#  (C) Copyright David Abrahams 2001. Permission to copy, use, modify, sell and
#  distribute this software is granted provided this copyright notice appears in
#  all copies. This software is provided "as is" without express or implied
#  warranty, and with no claim as to its suitability for any purpose.

""" Utility functions to add/remove/get grists.
    Grists are string enclosed in angle brackets (<>) that are used as prefixes. See Jam for more information.
"""

import re
import os
import bjam
from b2.exceptions import *
from b2.util import is_iterable_typed

__re_grist_and_value = re.compile (r'(<[^>]*>)(.*)')
__re_grist_content = re.compile ('^<(.*)>$')
__re_backslash = re.compile (r'\\')

def to_seq (value):
    """ If value is a sequence, returns it.
        If it is a string, returns a sequence with value as its sole element.
        """
    if not value:
        return []

    if isinstance (value, str):
        return [value]

    else:
        return value

def replace_references_by_objects (manager, refs):
    objs = []
    for r in refs:
        objs.append (manager.get_object (r))
    return objs

def add_grist (features):
    """ Transform a string by bracketing it with "<>". If already bracketed, does nothing.
        features: one string or a sequence of strings
        return: the gristed string, if features is a string, or a sequence of gristed strings, if features is a sequence
    """
    assert is_iterable_typed(features, basestring) or isinstance(features, basestring)
    def grist_one (feature):
        if feature [0] != '<' and feature [len (feature) - 1] != '>':
            return '<' + feature + '>'
        else:
            return feature

    if isinstance (features, str):
        return grist_one (features)
    else:
        return [ grist_one (feature) for feature in features ]

def replace_grist (features, new_grist):
    """ Replaces the grist of a string by a new one.
        Returns the string with the new grist.
    """
    assert is_iterable_typed(features, basestring) or isinstance(features, basestring)
    assert isinstance(new_grist, basestring)
    # this function is used a lot in the build phase and the original implementation
    # was extremely slow; thus some of the weird-looking optimizations for this function.
    single_item = False
    if isinstance(features, str):
        features = [features]
        single_item = True

    result = []
    for feature in features:
        # '<feature>value' -> ('<feature', '>', 'value')
        # 'something' -> ('something', '', '')
        # '<toolset>msvc/<feature>value' -> ('<toolset', '>', 'msvc/<feature>value')
        grist, split, value = feature.partition('>')
        # if a partition didn't occur, then grist is just 'something'
        # set the value to be the grist
        if not value and not split:
            value = grist
        result.append(new_grist + value)

    if single_item:
        return result[0]
    return result

def get_value (property):
    """ Gets the value of a property, that is, the part following the grist, if any.
    """
    assert is_iterable_typed(property, basestring) or isinstance(property, basestring)
    return replace_grist (property, '')

def get_grist (value):
    """ Returns the grist of a string.
        If value is a sequence, does it for every value and returns the result as a sequence.
    """
    assert is_iterable_typed(value, basestring) or isinstance(value, basestring)
    def get_grist_one (name):
        split = __re_grist_and_value.match (name)
        if not split:
            return ''
        else:
            return split.group (1)

    if isinstance (value, str):
        return get_grist_one (value)
    else:
        return [ get_grist_one (v) for v in value ]

def ungrist (value):
    """ Returns the value without grist.
        If value is a sequence, does it for every value and returns the result as a sequence.
    """
    assert is_iterable_typed(value, basestring) or isinstance(value, basestring)
    def ungrist_one (value):
        stripped = __re_grist_content.match (value)
        if not stripped:
            raise BaseException ("in ungrist: '%s' is not of the form <.*>" % value)

        return stripped.group (1)

    if isinstance (value, str):
        return ungrist_one (value)
    else:
        return [ ungrist_one (v) for v in value ]

def replace_suffix (name, new_suffix):
    """ Replaces the suffix of name by new_suffix.
        If no suffix exists, the new one is added.
    """
    assert isinstance(name, basestring)
    assert isinstance(new_suffix, basestring)
    split = os.path.splitext (name)
    return split [0] + new_suffix

def forward_slashes (s):
    """ Converts all backslashes to forward slashes.
    """
    assert isinstance(s, basestring)
    return s.replace('\\', '/')


def split_action_id (id):
    """ Splits an id in the toolset and specific rule parts. E.g.
        'gcc.compile.c++' returns ('gcc', 'compile.c++')
    """
    assert isinstance(id, basestring)
    split = id.split ('.', 1)
    toolset = split [0]
    name = ''
    if len (split) > 1:
        name = split [1]
    return (toolset, name)

def os_name ():
    result = bjam.variable("OS")
    assert(len(result) == 1)
    return result[0]

def platform ():
    return bjam.variable("OSPLAT")

def os_version ():
    return bjam.variable("OSVER")

def on_windows ():
    """ Returns true if running on windows, whether in cygwin or not.
    """
    if bjam.variable("NT"):
        return True

    elif bjam.variable("UNIX"):

        uname = bjam.variable("JAMUNAME")
        if uname and uname[0].startswith("CYGWIN"):
            return True

    return False
