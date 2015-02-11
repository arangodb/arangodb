import sys, os
import win32api
import tempfile
import unittest
import gc
import pythoncom
import winerror
import cStringIO as StringIO
from pythoncom import _GetInterfaceCount, _GetGatewayCount
import win32com
import logging

def CheckClean():
    # Ensure no lingering exceptions - Python should have zero outstanding
    # COM objects
    sys.exc_clear()
    c = _GetInterfaceCount()
    if c:
        print "Warning - %d com interface objects still alive" % c
    c = _GetGatewayCount()
    if c:
        print "Warning - %d com gateway objects still alive" % c

def RegisterPythonServer(filename, verbose=0):
    cmd = '%s "%s" > nul 2>&1' % (win32api.GetModuleFileName(0), filename)
    if verbose:
        print "Registering engine", filename
#       print cmd
    rc = os.system(cmd)
    if rc:
        print "Registration command was:"
        print cmd
        raise RuntimeError, "Registration of engine '%s' failed" % filename

def ExecuteShellCommand(cmd, testcase,
                        expected_output = None, # Set to '' to check for nothing
                        tracebacks_ok = 0, # OK if the output contains a t/b?
                        ):
    output_name = tempfile.mktemp('win32com_test')
    cmd = cmd + ' > "%s" 2>&1' % output_name
    rc = os.system(cmd)
    output = open(output_name, "r").read().strip()
    class Failed(Exception): pass
    try:
        if rc:
            raise Failed, "exit code was " + str(rc)
        if expected_output is not None and output != expected_output:
            raise Failed, \
                  "Expected output %r (got %r)" % (expected_output, output)
        if not tracebacks_ok and \
           output.find("Traceback (most recent call last)")>=0:
            raise Failed, "traceback in program output"
        return output
    except Failed, why:
        print "Failed to exec command '%r'" % cmd
        print "Failed as", why
        print "** start of program output **"
        print output
        print "** end of program output **"
        testcase.fail("Executing '%s' failed as %s" % (cmd, why))

def assertRaisesCOM_HRESULT(testcase, hresult, func, *args, **kw):
    try:
        func(*args, **kw)
    except pythoncom.com_error, details:
        if details[0]==hresult:
            return
    testcase.fail("Excepected COM exception with HRESULT 0x%x" % hresult)

class CaptureWriter:
    def __init__(self):
        self.old_err = self.old_out = None
        self.clear()
    def capture(self):
        self.clear()
        self.old_out = sys.stdout
        self.old_err = sys.stderr
        sys.stdout = sys.stderr = self
    def release(self):
        if self.old_out:
            sys.stdout = self.old_out
            self.old_out = None
        if self.old_err:
            sys.stderr = self.old_err
            self.old_err = None
    def clear(self):
        self.captured = []
    def write(self, msg):
        self.captured.append(msg)
    def get_captured(self):
        return "".join(self.captured)
    def get_num_lines_captured(self):
        return len("".join(self.captured).split("\n"))

class LeakTestCase(unittest.TestCase):
    def __init__(self, real_test):
        unittest.TestCase.__init__(self)
        self.real_test = real_test
        self.num_test_cases = 1
        self.num_leak_iters = 2 # seems to be enough!
        if hasattr(sys, "gettotalrefcount"):
            self.num_test_cases = self.num_test_cases + self.num_leak_iters
    def countTestCases(self):
        return self.num_test_cases
    def runTest(self):
        assert 0, "not used"
    def __call__(self, result = None):
        # Always ensure we don't leak gateways/interfaces
        gc.collect()
        ni = _GetInterfaceCount()
        ng = _GetGatewayCount()
        self.real_test(result)
        # Failed - no point checking anything else
        if result.shouldStop or not result.wasSuccessful():
            return
        self._do_leak_tests(result)
        gc.collect()
        lost_i = _GetInterfaceCount() - ni
        lost_g = _GetGatewayCount() - ng
        if lost_i or lost_g:
            msg = "%d interface objects and %d gateway objects leaked" \
                                                        % (lost_i, lost_g)
            result.addFailure(self.real_test, (AssertionError, msg, None))
    def _do_leak_tests(self, result = None):
        try:
            gtrc = sys.gettotalrefcount
        except AttributeError:
            return # can't do leak tests in this build
            def gtrc():
                return 0
        # Assume already called once, to prime any caches etc
        trc = gtrc()
        for i in range(self.num_leak_iters):
            self.real_test(result)
            if result.shouldStop:
                break
        del i # created after we remembered the refcount!
        # int division here means one or 2 stray references won't force 
        # failure, but one per loop 
        lost = (gtrc() - trc) // self.num_leak_iters
        if lost < 0:
            msg = "LeakTest: %s appeared to gain %d references!!" % (self.real_test, -lost)
            result.addFailure(self.real_test, (AssertionError, msg, None))
        if lost > 0:
            msg = "LeakTest: %s lost %d references" % (self.real_test, lost)
            result.addFailure(self.real_test, (AssertionError, msg, None))

class TestLoader(unittest.TestLoader):
    def loadTestsFromTestCase(self, testCaseClass):
        """Return a suite of all tests cases contained in testCaseClass"""
        leak_tests = []
        for name in self.getTestCaseNames(testCaseClass):
            real_test = testCaseClass(name)
            leak_test = self._getTestWrapper(real_test)
            leak_tests.append(leak_test)
        return self.suiteClass(leak_tests)
    def _getTestWrapper(self, test):
        no_leak_tests = getattr(test, "no_leak_tests", False)
        if no_leak_tests:
            print "Test says it doesn't want leak tests!"
            return test
        return LeakTestCase(test)
    def loadTestsFromModule(self, mod):
        if hasattr(mod, "suite"):
            return mod.suite()
        else:
            return unittest.TestLoader.loadTestsFromModule(self, mod)
    def loadTestsFromName(self, name, module=None):
        test = unittest.TestLoader.loadTestsFromName(self, name, module)
        if isinstance(test, unittest.TestSuite):
            pass # hmmm? print "Don't wrap suites yet!", test._tests
        elif isinstance(test, unittest.TestCase):
            test = self._getTestWrapper(test)
        else:
            print "XXX - what is", test
        return test

# Utilities to set the win32com logger to something what just captures
# records written and doesn't print them.
class LogHandler(logging.Handler):
    def __init__(self):
        self.emitted = []
        logging.Handler.__init__(self)
    def emit(self, record):
        self.emitted.append(record)

def setup_test_logger():
        old_log = getattr(win32com, "logger", None)
        win32com.logger = logging.Logger('test')
        handler = LogHandler()
        win32com.logger.addHandler(handler)
        return handler.emitted, old_log

def restore_test_logger(prev_logger):
    if prev_logger is None:
        del win32com.logger
    else:
        win32com.logger = prev_logger
    
# We used to override some of this (and may later!)
TestCase = unittest.TestCase

def CapturingFunctionTestCase(*args, **kw):
    real_test = _CapturingFunctionTestCase(*args, **kw)
    return LeakTestCase(real_test)

class _CapturingFunctionTestCase(unittest.FunctionTestCase):#, TestCaseMixin):
    def __call__(self, result=None):
        if result is None: result = self.defaultTestResult()
        writer = CaptureWriter()
        #self._preTest()
        writer.capture()
        try:
            unittest.FunctionTestCase.__call__(self, result)
            if getattr(self, "do_leak_tests", 0) and hasattr(sys, "gettotalrefcount"):
                self.run_leak_tests(result)
        finally:
            writer.release()
            #self._postTest(result)
        output = writer.get_captured()
        self.checkOutput(output, result)
        if result.showAll:
            print output
    def checkOutput(self, output, result):
        if output.find("Traceback")>=0:
            msg = "Test output contained a traceback\n---\n%s\n---" % output
            result.errors.append((self, msg))

class ShellTestCase(unittest.TestCase):
    def __init__(self, cmd, expected_output):
        self.__cmd = cmd
        self.__eo = expected_output
        unittest.TestCase.__init__(self)
    def runTest(self):
        ExecuteShellCommand(self.__cmd, self, self.__eo)
    def __str__(self):
        max = 30
        if len(self.__cmd)>max:
            cmd_repr = self.__cmd[:max] + "..."
        else:
            cmd_repr = self.__cmd
        return "exec: " + cmd_repr

def testmain(*args, **kw):
    new_kw = kw.copy()
    if not new_kw.has_key('testLoader'):
        new_kw['testLoader'] = TestLoader()
    unittest.main(*args, **new_kw)
    CheckClean()
