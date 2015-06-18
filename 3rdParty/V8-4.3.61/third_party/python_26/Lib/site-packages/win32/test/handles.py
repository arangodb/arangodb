import sys
import unittest
import pywintypes
import win32api

# A class that will never die vie refcounting, but will die via GC.
class Cycle:
    def __init__(self, handle):
        self.cycle = self
        self.handle = handle

class PyHandleTestCase(unittest.TestCase):
    def testCleanup1(self):
        # We used to clobber all outstanding exceptions.
        def f1(invalidate):
            import win32event
            h = win32event.CreateEvent(None, 0, 0, None)
            if invalidate:
                win32api.CloseHandle(int(h))
            1/0
            # If we invalidated, then the object destruction code will attempt 
            # to close an invalid handle.  We don't wan't an exception in 
            # this case

        def f2(invalidate):
            """ This function should throw an IOError. """
            try:
                f1(invalidate)
            except ZeroDivisionError, exc:
                raise IOError("raise 2")

        self.assertRaises(IOError, f2, False)
        # Now do it again, but so the auto object destruction
        # actually fails.
        self.assertRaises(IOError, f2, True)

    def testCleanup2(self):
        # Cause an exception during object destruction.
        # The worst this does is cause an ".XXX undetected error (why=3)" 
        # So avoiding that is the goal
        import win32event
        h = win32event.CreateEvent(None, 0, 0, None)
        # Close the handle underneath the object.
        win32api.CloseHandle(int(h))
        # Object destructor runs with the implicit close failing
        h = None

    def testCleanup3(self):
        # And again with a class - no __del__
        import win32event
        class Test:
            def __init__(self):
                self.h = win32event.CreateEvent(None, 0, 0, None)
                win32api.CloseHandle(int(self.h))
        t=Test()
        t = None

    def testCleanupGood(self):
        # And check that normal error semantics *do* work.
        import win32event
        h = win32event.CreateEvent(None, 0, 0, None)
        win32api.CloseHandle(int(h))
        self.assertRaises(win32api.error, h.Close)
        # A following Close is documented as working
        h.Close()

    def testInvalid(self):
        h=pywintypes.HANDLE(-2)
        self.assertRaises(win32api.error, h.Close)

    def testOtherHandle(self):
        h=pywintypes.HANDLE(1)
        h2=pywintypes.HANDLE(h)
        self.failUnlessEqual(h, h2)
        # but the above doesn't really test everything - we want a way to
        # pass the handle directly into PyWinLong_AsVoidPtr.  One way to
        # to that is to abuse win32api.GetProcAddress() - the 2nd param
        # is passed to PyWinLong_AsVoidPtr() if its not a string.
        # passing a handle value of '1' should work - there is something
        # at that ordinal
        win32api.GetProcAddress(sys.dllhandle, h)

    def testLong(self):
        # sys.maxint+1 should always be a 'valid' handle, treated as an
        # unsigned int, even though it is a long. Although pywin32 should not
        # directly create such longs, using struct.unpack() with a P format
        # may well return them. eg:
        # >>> struct.unpack("P", struct.pack("P", -1))
        # (4294967295L,)
        pywintypes.HANDLE(sys.maxint+1)

    def testGC(self):
        # This used to provoke:
        # Fatal Python error: unexpected exception during garbage collection
        def make():
            h=pywintypes.HANDLE(-2)
            c = Cycle(h)
        import gc
        make()
        gc.collect()

    def testTypes(self):
        self.assertRaises(TypeError, pywintypes.HANDLE, "foo")
        self.assertRaises(TypeError, pywintypes.HANDLE, ())
        # should be able to get a long!
        pywintypes.HANDLE(0L)

if __name__ == '__main__':
    unittest.main()
