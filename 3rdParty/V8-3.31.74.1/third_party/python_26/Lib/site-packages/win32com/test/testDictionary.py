# testDictionary.py
#
import sys
import win32com.server.util
import win32com.test.util
import win32com.client
import traceback
import pythoncom
import pywintypes
import winerror
L=pywintypes.Unicode

import unittest

error = "dictionary test error"

def MakeTestDictionary():
    return win32com.client.Dispatch("Python.Dictionary")

def TestDictAgainst(dict,check):
    for key, value in check.items():
        if dict(key) != value:
            raise error, "Indexing for '%s' gave the incorrect value - %s/%s" % (`key`, `dict[key]`, `check[key]`)

# Ensure we have the correct version registered.
def Register(quiet):
    import win32com.server.register
    from win32com.servers.dictionary import DictionaryPolicy
    win32com.server.register.RegisterClasses(DictionaryPolicy, quiet=quiet)

def TestDict(quiet=None):
    if quiet is None:
        quiet = not "-v" in sys.argv
    Register(quiet)

    if not quiet: print "Simple enum test"
    dict = MakeTestDictionary()
    checkDict = {}
    TestDictAgainst(dict, checkDict)

    dict["NewKey"] = "NewValue"
    checkDict["NewKey"] = "NewValue"
    TestDictAgainst(dict, checkDict)

    dict["NewKey"] = None
    del checkDict["NewKey"]
    TestDictAgainst(dict, checkDict)

    if not quiet:
        print "Failure tests"
    try:
        dict()
        raise error, "default method with no args worked when it shouldnt have!"
    except pythoncom.com_error, (hr, desc, exc, argErr):
        if hr != winerror.DISP_E_BADPARAMCOUNT:
            raise error, "Expected DISP_E_BADPARAMCOUNT - got %d (%s)" % (hr, desc)

    try:
        dict("hi", "there")
        raise error, "multiple args worked when it shouldnt have!"
    except pythoncom.com_error, (hr, desc, exc, argErr):
        if hr != winerror.DISP_E_BADPARAMCOUNT:
            raise error, "Expected DISP_E_BADPARAMCOUNT - got %d (%s)" % (hr, desc)

    try:
        dict(0)
        raise error, "int key worked when it shouldnt have!"
    except pythoncom.com_error, (hr, desc, exc, argErr):
        if hr != winerror.DISP_E_TYPEMISMATCH:
            raise error, "Expected DISP_E_TYPEMISMATCH - got %d (%s)" % (hr, desc)

    if not quiet:
        print "Python.Dictionary tests complete."

class TestCase(win32com.test.util.TestCase):
    def testDict(self):
        TestDict()

if __name__=='__main__':
    unittest.main()
