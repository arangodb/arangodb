import sys, os
import unittest

# A list of demos that depend on user-interface of *any* kind.  Tests listed
# here are not suitable for unattended testing.
ui_demos = """GetSaveFileName print_desktop win32cred_demo win32gui_demo
              win32gui_dialog win32gui_menu win32gui_taskbar
              win32rcparser_demo winprocess win32console_demo""".split()
# Other demos known as 'bad' (or at least highly unlikely to work)
# cerapi: no CE module is built (CE via pywin32 appears dead)
# desktopmanager: hangs (well, hangs for 60secs or so...)
bad_demos = "cerapi desktopmanager win32comport_demo".split()

argvs = {
    "rastest": ("-l",),
}

ok_exceptions = {
    "RegCreateKeyTransacted": ("NotImplementedError"),
}

class TestRunner:
    def __init__(self, argv):
        self.argv = argv
    def __call__(self):
        # subprocess failed in strange ways for me??
        fin, fout, ferr = os.popen3(" ".join(self.argv))
        fin.close()
        output = fout.read() + ferr.read()
        fout.close()
        rc = ferr.close()
        if rc:
            base = os.path.basename(self.argv[1])
            raise AssertionError, "%s failed with exit code %s.  Output is:\n%s" % (base, rc, output)

def get_demo_tests():
    import win32api
    ret = []
    demo_dir = os.path.abspath(os.path.join(os.path.dirname(win32api.__file__), "Demos"))
    assert os.path.isdir(demo_dir), demo_dir
    for name in os.listdir(demo_dir):
        base, ext = os.path.splitext(name)
        if ext != ".py" or base in ui_demos or base in bad_demos:
            continue
        if base in ok_exceptions:
            print "Ack - can't handle test %s - can't catch specific exceptions" % (base,)
            continue
        argv = (sys.executable, os.path.join(demo_dir, base+".py")) + \
               argvs.get(base, ())
        ret.append(unittest.FunctionTestCase(TestRunner(argv), description="win32/demos/" + name))
    return ret

def import_all():
    # Some hacks for import order - dde depends on win32ui
    try:
        import win32ui
    except ImportError:
        pass # 'what-ev-a....'
    
    import win32api
    dir = os.path.dirname(win32api.__file__)
    num = 0
    is_debug = os.path.basename(win32api.__file__).endswith("_d")
    for name in os.listdir(dir):
        base, ext = os.path.splitext(name)
        if (ext==".pyd") and \
           name != "_winxptheme.pyd" and \
           (is_debug and base.endswith("_d") or \
           not is_debug and not base.endswith("_d")):
            try:
                __import__(base)
            except ImportError:
                print "FAILED to import", name
                raise
            num += 1

def suite():
    # Loop over all .py files here, except me :)
    try:
        me = __file__
    except NameError:
        me = sys.argv[0]
    me = os.path.abspath(me)
    files = os.listdir(os.path.dirname(me))
    suite = unittest.TestSuite()
    suite.addTest(unittest.FunctionTestCase(import_all))
    for file in files:
        base, ext = os.path.splitext(file)
        if ext=='.py' and os.path.basename(me) != file:
            try:
                mod = __import__(base)
            except ImportError, why:
                print "FAILED to import test module"
                print why
                continue
            if hasattr(mod, "suite"):
                test = mod.suite()
            else:
                test = unittest.defaultTestLoader.loadTestsFromModule(mod)
            suite.addTest(test)
    for test in get_demo_tests():
        suite.addTest(test)
    return suite

class CustomLoader(unittest.TestLoader):
    def loadTestsFromModule(self, module):
        return suite()

if __name__=='__main__':
    unittest.TestProgram(testLoader=CustomLoader())(argv=sys.argv)
