#  (C) Copyright David Abrahams 2001. Permission to copy, use, modify, sell and
#  distribute this software is granted provided this copyright notice appears in
#  all copies. This software is provided "as is" without express or implied
#  warranty, and with no claim as to its suitability for any purpose.

from b2.util import is_iterable
from .utility import to_seq


def difference (b, a):
    """ Returns the elements of B that are not in A.
    """
    a = set(a)
    result = []
    for item in b:
        if item not in a:
            result.append(item)
    return result

def intersection (set1, set2):
    """ Removes from set1 any items which don't appear in set2 and returns the result.
    """
    assert is_iterable(set1)
    assert is_iterable(set2)
    result = []
    for v in set1:
        if v in set2:
            result.append (v)
    return result

def contains (small, large):
    """ Returns true iff all elements of 'small' exist in 'large'.
    """
    small = to_seq (small)
    large = to_seq (large)

    for s in small:
        if not s in large:
            return False
    return True

def equal (a, b):
    """ Returns True iff 'a' contains the same elements as 'b', irrespective of their order.
        # TODO: Python 2.4 has a proper set class.
    """
    assert is_iterable(a)
    assert is_iterable(b)
    return contains (a, b) and contains (b, a)
