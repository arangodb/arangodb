import sys, os
import struct
import unittest
import copy

import win32con
import pythoncom
from win32com.shell import shell
from win32com.shell.shellcon import *
from win32com.storagecon import *

import win32com.test.util

class ShellTester(win32com.test.util.TestCase):
    def testShellLink(self):
        desktop = str(shell.SHGetSpecialFolderPath(0, CSIDL_DESKTOP))
        num = 0
        shellLink = pythoncom.CoCreateInstance(shell.CLSID_ShellLink, None, pythoncom.CLSCTX_INPROC_SERVER, shell.IID_IShellLink)
        persistFile = shellLink.QueryInterface(pythoncom.IID_IPersistFile)
        for base_name in os.listdir(desktop):
            name = os.path.join(desktop, base_name)
            try:
                persistFile.Load(name,STGM_READ)
            except pythoncom.com_error:
                continue
            # Resolve is slow - avoid it for our tests.
            #shellLink.Resolve(0, shell.SLR_ANY_MATCH | shell.SLR_NO_UI)
            fname, findData = shellLink.GetPath(0)
            unc = shellLink.GetPath(shell.SLGP_UNCPRIORITY)[0]
            num += 1
        if num == 0:
            # This isn't a fatal error, but is unlikely.
            print "Could not find any links on your desktop, which is unusual"

    def testShellFolder(self):
        sf = shell.SHGetDesktopFolder()
        names_1 = []
        for i in sf: # Magically calls EnumObjects
            name = sf.GetDisplayNameOf(i, SHGDN_NORMAL)
            names_1.append(name)
        
        # And get the enumerator manually    
        enum = sf.EnumObjects(0, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN)
        names_2 = []
        for i in enum:
            name = sf.GetDisplayNameOf(i, SHGDN_NORMAL)
            names_2.append(name)
        names_1.sort()
        names_2.sort()
        self.assertEqual(names_1, names_2)

class PIDLTester(win32com.test.util.TestCase):
    def _rtPIDL(self, pidl):
        pidl_str = shell.PIDLAsString(pidl)
        pidl_rt = shell.StringAsPIDL(pidl_str)
        self.assertEqual(pidl_rt, pidl)
        pidl_str_rt = shell.PIDLAsString(pidl_rt)
        self.assertEqual(pidl_str_rt, pidl_str)

    def _rtCIDA(self, parent, kids):
        cida = parent, kids
        cida_str = shell.CIDAAsString(cida)
        cida_rt = shell.StringAsCIDA(cida_str)
        self.assertEqual(cida, cida_rt)
        cida_str_rt = shell.CIDAAsString(cida_rt)
        self.assertEqual(cida_str_rt, cida_str)

    def testPIDL(self):
        # A PIDL of "\1" is:   cb    pidl   cb
        expect =            "\03\00" "\1"  "\0\0"
        self.assertEqual(shell.PIDLAsString(["\1"]), expect)
        self._rtPIDL(["\0"])
        self._rtPIDL(["\1", "\2", "\3"])
        self._rtPIDL(["\0" * 2048] * 2048)
        # PIDL must be a list
        self.assertRaises(TypeError, shell.PIDLAsString, "foo")

    def testCIDA(self):
        self._rtCIDA(["\0"], [ ["\0"] ])
        self._rtCIDA(["\1"], [ ["\2"] ])
        self._rtCIDA(["\0"], [ ["\0"], ["\1"], ["\2"] ])

    def testBadShortPIDL(self):
        # A too-short child element:   cb    pidl   cb
        pidl =                       "\01\00" "\1"
        self.assertRaises(ValueError, shell.StringAsPIDL, pidl)

        # ack - tried to test too long PIDLs, but a len of 0xFFFF may not
        # always fail.

class FILEGROUPDESCRIPTORTester(win32com.test.util.TestCase):
    def _testRT(self, fd):
        fgd_string = shell.FILEGROUPDESCRIPTORAsString([fd])
        fd2 = shell.StringAsFILEGROUPDESCRIPTOR(fgd_string)[0]
        
        fd = fd.copy()
        fd2 = fd2.copy()
        
        # The returned objects *always* have dwFlags and cFileName.
        if not fd.has_key('dwFlags'):
            del fd2['dwFlags']
        if not fd.has_key('cFileName'):
            self.assertEqual(fd2['cFileName'], '')
            del fd2['cFileName']

        self.assertEqual(fd, fd2)

    def testSimple(self):
        fgd = shell.FILEGROUPDESCRIPTORAsString([])
        header = struct.pack("i", 0)
        self.assertEqual(header, fgd[:len(header)])
        self._testRT(dict())
        d = dict()
        fgd = shell.FILEGROUPDESCRIPTORAsString([d])
        header = struct.pack("i", 1)
        self.assertEqual(header, fgd[:len(header)])
        self._testRT(d)
    
    def testComplex(self):
        if sys.hexversion < 0x2030000:
            # no kw-args to dict in 2.2 - not worth converting!
            return
        clsid = pythoncom.MakeIID("{CD637886-DB8B-4b04-98B5-25731E1495BE}")
        d = dict(cFileName="foo.txt",
                 clsid=clsid,
                 sizel=(1,2),
                 pointl=(3,4),
                 dwFileAttributes = win32con.FILE_ATTRIBUTE_NORMAL,
                 ftCreationTime=pythoncom.MakeTime(10),
                 ftLastAccessTime=pythoncom.MakeTime(11),
                 ftLastWriteTime=pythoncom.MakeTime(12),
                 nFileSize=sys.maxint + 1)
        self._testRT(d)

    def testUnicode(self):
        # exercise a bug fixed in build 210 - multiple unicode objects failed.
        if sys.hexversion < 0x2030000:
            # no kw-args to dict in 2.2 - not worth converting!
            return
        d = [dict(cFileName=u"foo.txt",
                 sizel=(1,2),
                 pointl=(3,4),
                 dwFileAttributes = win32con.FILE_ATTRIBUTE_NORMAL,
                 ftCreationTime=pythoncom.MakeTime(10),
                 ftLastAccessTime=pythoncom.MakeTime(11),
                 ftLastWriteTime=pythoncom.MakeTime(12),
                 nFileSize=sys.maxint + 1),
            dict(cFileName=u"foo2.txt",
                 sizel=(1,2),
                 pointl=(3,4),
                 dwFileAttributes = win32con.FILE_ATTRIBUTE_NORMAL,
                 ftCreationTime=pythoncom.MakeTime(10),
                 ftLastAccessTime=pythoncom.MakeTime(11),
                 ftLastWriteTime=pythoncom.MakeTime(12),
                 nFileSize=sys.maxint + 1),
            dict(cFileName=u"foo\xa9.txt",
                 sizel=(1,2),
                 pointl=(3,4),
                 dwFileAttributes = win32con.FILE_ATTRIBUTE_NORMAL,
                 ftCreationTime=pythoncom.MakeTime(10),
                 ftLastAccessTime=pythoncom.MakeTime(11),
                 ftLastWriteTime=pythoncom.MakeTime(12),
                 nFileSize=sys.maxint + 1),
            ]
        s = shell.FILEGROUPDESCRIPTORAsString(d, 1)
        d2 = shell.StringAsFILEGROUPDESCRIPTOR(s)
        # clobber 'dwFlags' - they are not expected to be identical
        for t in d2:
            del t['dwFlags']
        self.assertEqual(d, d2)

class FileOperationTester(win32com.test.util.TestCase):
    def setUp(self):
        import tempfile
        self.src_name = os.path.join(tempfile.gettempdir(), "pywin32_testshell")
        self.dest_name = os.path.join(tempfile.gettempdir(), "pywin32_testshell_dest")
        self.test_data = "Hello from\0Python"
        f=file(self.src_name, "wb")
        f.write(self.test_data)
        f.close()
        try:
            os.unlink(self.dest_name)
        except os.error:
            pass

    def tearDown(self):
        for fname in (self.src_name, self.dest_name):
            if os.path.isfile(fname):
                os.unlink(fname)

    def testCopy(self):
        s = (0, # hwnd,
             FO_COPY, #operation
             self.src_name,
             self.dest_name)

        rc, aborted = shell.SHFileOperation(s)
        self.failUnless(not aborted)
        self.failUnlessEqual(0, rc)
        self.failUnless(os.path.isfile(self.src_name))
        self.failUnless(os.path.isfile(self.dest_name))

    def testRename(self):
        s = (0, # hwnd,
             FO_RENAME, #operation
             self.src_name,
             self.dest_name)
        rc, aborted = shell.SHFileOperation(s)
        self.failUnless(not aborted)
        self.failUnlessEqual(0, rc)
        self.failUnless(os.path.isfile(self.dest_name))
        self.failUnless(not os.path.isfile(self.src_name))

    def testMove(self):
        s = (0, # hwnd,
             FO_MOVE, #operation
             self.src_name,
             self.dest_name)
        rc, aborted = shell.SHFileOperation(s)
        self.failUnless(not aborted)
        self.failUnlessEqual(0, rc)
        self.failUnless(os.path.isfile(self.dest_name))
        self.failUnless(not os.path.isfile(self.src_name))

    def testDelete(self):
        s = (0, # hwnd,
             FO_DELETE, #operation
             self.src_name, None,
             FOF_NOCONFIRMATION)
        rc, aborted = shell.SHFileOperation(s)
        self.failUnless(not aborted)
        self.failUnlessEqual(0, rc)
        self.failUnless(not os.path.isfile(self.src_name))

if __name__=='__main__':
    win32com.test.util.testmain()
