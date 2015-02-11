# A sample of using Vista's IExplorerBrowser interfaces...
# Currently doesn't quite work:
# * CPU sits at 100% while running.

import pythoncom
from win32com.shell import shell, shellcon
import win32gui, win32con, win32api
from win32com.server.util import wrap, unwrap

# event handler for the browser.
IExplorerBrowserEvents_Methods = """OnNavigationComplete OnNavigationFailed 
                                    OnNavigationPending OnViewCreated""".split()
class EventHandler:
    _com_interfaces_ = [shell.IID_IExplorerBrowserEvents]
    _public_methods_ = IExplorerBrowserEvents_Methods

    def OnNavigationComplete(self, pidl):
        print "OnNavComplete", pidl

    def OnNavigationFailed(self, pidl):
        print "OnNavigationFailed", pidl

    def OnNavigationPending(self, pidl):
        print "OnNavigationPending", pidl

    def OnViewCreated(self, view):
        print "OnViewCreated", view
        # And if our demo view has been registered, it may well
        # be that view!
        try:
            pyview = unwrap(view)
            print "and look - its a Python implemented view!", pyview
        except ValueError:
            pass

class MainWindow:
    def __init__(self):
        message_map = {
                win32con.WM_DESTROY: self.OnDestroy,
                win32con.WM_COMMAND: self.OnCommand,
                win32con.WM_SIZE: self.OnSize,
        }
        # Register the Window class.
        wc = win32gui.WNDCLASS()
        hinst = wc.hInstance = win32api.GetModuleHandle(None)
        wc.lpszClassName = "test_explorer_browser"
        wc.lpfnWndProc = message_map # could also specify a wndproc.
        classAtom = win32gui.RegisterClass(wc)
        # Create the Window.
        style = win32con.WS_OVERLAPPEDWINDOW | win32con.WS_VISIBLE
        self.hwnd = win32gui.CreateWindow( classAtom, "Python IExplorerBrowser demo", style, \
                0, 0, win32con.CW_USEDEFAULT, win32con.CW_USEDEFAULT, \
                0, 0, hinst, None)
        eb = pythoncom.CoCreateInstance(shellcon.CLSID_ExplorerBrowser, None, pythoncom.CLSCTX_ALL, shell.IID_IExplorerBrowser)
        # as per MSDN docs, hook up events early
        self.event_cookie = eb.Advise(wrap(EventHandler()))

        eb.SetOptions(shellcon.EBO_SHOWFRAMES)
        rect = win32gui.GetClientRect(self.hwnd)
        # Set the flags such that the folders autoarrange and non web view is presented
        flags = (shellcon.FVM_LIST, shellcon.FWF_AUTOARRANGE | shellcon.FWF_NOWEBVIEW)
        eb.Initialize(self.hwnd, rect, (0, shellcon.FVM_DETAILS))
        # And start browsing at the root of the namespace.
        eb.BrowseToIDList([], shellcon.SBSP_ABSOLUTE)
        #eb.FillFromObject(None, shellcon.EBF_NODROPTARGET); 
        #eb.SetEmptyText("No known folders yet...");  
        self.eb = eb

    def OnCommand(self, hwnd, msg, wparam, lparam):
        pass

    def OnDestroy(self, hwnd, msg, wparam, lparam):
        print "tearing down ExplorerBrowser..."
        self.eb.Unadvise(self.event_cookie)
        self.eb.Destroy()
        self.eb = None
        print "shutting down app..."
        win32gui.PostQuitMessage(0)

    def OnSize(self, hwnd, msg, wparam, lparam):
        x = win32api.LOWORD(lparam)
        y = win32api.HIWORD(lparam)
        self.eb.SetRect(None, (0, 0, x, y))

def main():
    w=MainWindow()
    win32gui.PumpMessages()

if __name__=='__main__':
    main()
