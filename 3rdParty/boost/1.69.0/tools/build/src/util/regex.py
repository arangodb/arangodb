#  (C) Copyright David Abrahams 2001. Permission to copy, use, modify, sell and
#  distribute this software is granted provided this copyright notice appears in
#  all copies. This software is provided "as is" without express or implied
#  warranty, and with no claim as to its suitability for any purpose.

import re

from b2.util import bjam_signature


def transform (list, pattern, indices = [1]):
    """ Matches all elements of 'list' against the 'pattern'
        and returns a list of the elements indicated by indices of
        all successful matches. If 'indices' is omitted returns
        a list of first paranthethised groups of all successful
        matches.
    """
    result = []

    for e in list:
        m = re.match (pattern, e)

        if m:
            for i in indices:
                result.append (m.group (i))

    return result


@bjam_signature([['s', 'pattern', 'replacement']])
def replace(s, pattern, replacement):
    """Replaces occurrences of a match string in a given
    string and returns the new string. The match string
    can be a regex expression.

    Args:
        s (str):           the string to modify
        pattern (str):     the search expression
        replacement (str): the string to replace each match with
    """
    # the replacement string may contain invalid backreferences (like \1 or \g)
    # which will cause python's regex to blow up. Since this should emulate
    # the jam version exactly and the jam version didn't support
    # backreferences, this version shouldn't either. re.sub
    # allows replacement to be a callable; this is being used
    # to simply return the replacement string and avoid the hassle
    # of worrying about backreferences within the string.
    def _replacement(matchobj):
        return replacement
    return re.sub(pattern, _replacement, s)


@bjam_signature((['items', '*'], ['match'], ['replacement']))
def replace_list(items, match, replacement):
    """Replaces occurrences of a match string in a given list of strings and returns
    a list of new strings. The match string can be a regex expression.

    Args:
        items (list):       the list of strings to modify.
        match (str):        the search expression.
        replacement (str):  the string to replace with.
    """
    return [replace(item, match, replacement) for item in items]
