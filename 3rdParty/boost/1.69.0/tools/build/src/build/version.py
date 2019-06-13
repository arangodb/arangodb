import os
import sys

import bjam


from b2.manager import get_manager


MANAGER = get_manager()
ERROR_HANDLER = MANAGER.errors()

_major = "2015"
_minor = "07"


def boost_build():
    return "{}.{}-git".format(_major, _minor)


def verify_engine_version():
    major, minor, _ = v = bjam.variable('JAM_VERSION')
    if major != _major or minor != _minor:
        from textwrap import dedent
        engine = sys.argv[0]
        core = os.path.dirname(os.path.dirname(__file__))
        print dedent("""\
        warning: mismatched version of Boost.Build engine core
        warning: Boost.Build engine "{}" is "{}"
        warning: Boost.Build core at {} is {}
        """.format(engine, '.'.join(v), core, boost_build()))
        return False
    return True


def report():
    if verify_engine_version():
        print "Boost.Build " + boost_build()
