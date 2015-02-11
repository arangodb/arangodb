import pythoncom
import unittest
import win32com.test.util

class TestROT(win32com.test.util.TestCase):
    def testit(self):
        ctx = pythoncom.CreateBindCtx()
        rot = pythoncom.GetRunningObjectTable()
        num = 0
        for mk in rot:
            name = mk.GetDisplayName(ctx, None)
            num += 1
            # Monikers themselves can iterate their contents
            for sub in mk:
                num += 1

        #if num < 2:
        #    print "Only", num, "objects in the ROT - this is unusual"

if __name__=='__main__':
    unittest.main()
