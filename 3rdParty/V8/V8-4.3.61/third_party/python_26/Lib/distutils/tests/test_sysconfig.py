"""Tests for distutils.dist."""

from distutils import sysconfig
import os
import unittest

from test.test_support import TESTFN

class SysconfigTestCase(unittest.TestCase):

    def test_get_config_h_filename(self):
        config_h = sysconfig.get_config_h_filename()
        self.assert_(os.path.isfile(config_h), config_h)

    def test_get_python_lib(self):
        lib_dir = sysconfig.get_python_lib()
        # XXX doesn't work on Linux when Python was never installed before
        #self.assert_(os.path.isdir(lib_dir), lib_dir)
        # test for pythonxx.lib?
        self.assertNotEqual(sysconfig.get_python_lib(),
                            sysconfig.get_python_lib(prefix=TESTFN))

    def test_get_python_inc(self):
        # NOTE: See bugs 4151, 4070 of the py tracker. The patch
        # proposed in 4151 does lots more changes to setup.py,
        # sysconfig.py and other tests. I cannot say how much these
        # would influence the build at large, so I ignored these
        # parts. What is now left of this function is identical to
        # that patch, functionality-wise.
        inc_dir = sysconfig.get_python_inc()
        self.assert_(os.path.isdir(inc_dir), inc_dir)
        python_h = os.path.join(inc_dir, "Python.h")
        self.assert_(os.path.isfile(python_h), python_h)

    def test_get_config_vars(self):
        cvars = sysconfig.get_config_vars()
        self.assert_(isinstance(cvars, dict))
        self.assert_(cvars)


def test_suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(SysconfigTestCase))
    return suite
