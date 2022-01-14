# Copyright David Abrahams 2004. Distributed under the Boost
# Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
'''
>>> from long_ext import *
>>> print(new_long())
0
>>> print(longify(42))
42
>>> print(longify_string('300'))
300
>>> is_long(long(20))
'yes'
>>> is_long('20')
0

>>> x = Y(long(4294967295))
'''

import sys
if (sys.version_info.major >= 3):
    long = int

def run(args = None):
    import sys
    import doctest

    if args is not None:
        sys.argv = args
    return doctest.testmod(sys.modules.get(__name__))

if __name__ == '__main__':
    print("running...")
    import sys
    status = run()[0]
    if (status == 0): print("Done.")
    sys.exit(status)
