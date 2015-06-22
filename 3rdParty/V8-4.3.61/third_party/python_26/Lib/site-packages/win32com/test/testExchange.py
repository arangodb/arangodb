# TestExchange = Exchange Server Dump
# Note that this code uses "CDO", which is unlikely to get the best choice.
# You should use the Outlook object model, or
# the win32com.mapi examples for a low-level interface.

from win32com.client import gencache, constants
import pythoncom
import os

ammodule = gencache.EnsureModule('{3FA7DEA7-6438-101B-ACC1-00AA00423326}', 0, 1, 1)

def GetDefaultProfileName():
    import win32api, win32con
    try:
        key = win32api.RegOpenKey(win32con.HKEY_CURRENT_USER, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows Messaging Subsystem\\Profiles")
        try:
            return win32api.RegQueryValueEx(key, "DefaultProfile")[0]
        finally:
            key.Close()
    except win32api.error:
        return None

#
# Recursive dump of folders.
#
def DumpFolder(folder, indent = 0):
    print " " * indent, folder.Name
    folders = folder.Folders
    folder = folders.GetFirst()
    while folder:
        DumpFolder(folder, indent+1)
        folder = folders.GetNext()

def DumpFolders(session):
    infostores = session.InfoStores
    print infostores
    print "There are %d infostores" % infostores.Count
    for i in range(infostores.Count):
        infostore = infostores[i+1]
        print "Infostore = ", infostore.Name
        try:
            folder = infostore.RootFolder
        except pythoncom.com_error, details:
            hr, msg, exc, arg = details
            # -2147221219 == MAPI_E_FAILONEPROVIDER - a single provider temporarily not available.
            if exc and exc[-1]==-2147221219:
                print "This info store is currently not available"
                continue
        DumpFolder(folder)

# Build a dictionary of property tags, so I can reverse look-up
#
PropTagsById={}
if ammodule:
    for name, val in ammodule.constants.__dict__.items():
        PropTagsById[val] = name


def TestAddress(session):
#       entry = session.GetAddressEntry("Skip")
#       print entry
    pass


def TestUser(session):
    ae = session.CurrentUser
    fields = ae.Fields
    print "User has %d fields" % fields.Count
    for f in range(len(fields)):
        field = fields[f+1]
        try:
            id = PropTagsById[field.ID]
        except KeyError:
            id = field.ID
        print "%s/%s=%s" % (field.Name, id, field.Value)

def test():
    if not ammodule:
        print "MAPI does not appear to be installed on this machine - skipping."
        return

    import win32com.client
    oldcwd = os.getcwd()
    session = win32com.client.Dispatch("MAPI.Session")
    try:
        session.Logon(GetDefaultProfileName())
    except pythoncom.com_error, details:
        print "Could not log on to MAPI:", details
        return
    try:
        TestUser(session)
        TestAddress(session)
        DumpFolders(session)
    finally:
        session.Logoff()
        # It appears Exchange will change the cwd on us :(
        os.chdir(oldcwd)

if __name__=='__main__':
    from util import CheckClean
    test()
    CheckClean()
