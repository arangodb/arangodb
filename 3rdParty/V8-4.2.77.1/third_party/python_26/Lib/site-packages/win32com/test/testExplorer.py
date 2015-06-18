# testExplorer -

import string
import sys
import os
import win32com.client.dynamic
import win32api
import glob
import pythoncom
import time
from util import CheckClean

bVisibleEventFired = 0

class ExplorerEvents:
    def OnVisible(self, visible):
        global bVisibleEventFired
        bVisibleEventFired = 1

def TestExplorerEvents():
    global bVisibleEventFired
    iexplore = win32com.client.DispatchWithEvents("InternetExplorer.Application", ExplorerEvents)
    iexplore.Visible = 1
    if not bVisibleEventFired:
        raise RuntimeError, "The IE event did not appear to fire!"
    iexplore.Quit()
    iexplore = None

    bVisibleEventFired = 0
    ie = win32com.client.Dispatch("InternetExplorer.Application")
    ie_events = win32com.client.DispatchWithEvents(ie, ExplorerEvents)
    ie.Visible = 1
    if not bVisibleEventFired:
        raise RuntimeError, "The IE event did not appear to fire!"
    ie.Quit()
    ie = None
    print "IE Event tests worked."


def TestExplorer(iexplore):
    if not iexplore.Visible: iexplore.Visible = -1
    try:
        iexplore.Navigate(win32api.GetFullPathName('..\\readme.htm'))
    except pythoncom.com_error, details:
        print "Warning - could not open the test HTML file", details
#       for fname in glob.glob("..\\html\\*.html"):
#               print "Navigating to", fname
#               while iexplore.Busy:
#                       win32api.Sleep(100)
#               iexplore.Navigate(win32api.GetFullPathName(fname))
    win32api.Sleep(4000)
    try:
        iexplore.Quit()
    except (AttributeError, pythoncom.com_error):
        # User got sick of waiting :)
        pass

def TestAll():
    try:
        iexplore = win32com.client.dynamic.Dispatch("InternetExplorer.Application")
        TestExplorer(iexplore)

        win32api.Sleep(1000)
        iexplore = None

        # Test IE events.
        TestExplorerEvents()
        # Give IE a chance to shutdown, else it can get upset on fast machines.
        time.sleep(2)

        # Note that the TextExplorerEvents will force makepy - hence
        # this gencache is really no longer needed.

        from win32com.client import gencache
        gencache.EnsureModule("{EAB22AC0-30C1-11CF-A7EB-0000C05BAE0B}", 0, 1, 1)
        iexplore = win32com.client.Dispatch("InternetExplorer.Application")
        TestExplorer(iexplore)

    finally:
        iexplore = None

if __name__=='__main__':
    TestAll()
    CheckClean()
