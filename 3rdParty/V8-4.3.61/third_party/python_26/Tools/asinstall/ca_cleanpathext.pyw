"""
    Python Custom Action script for Windows installer to clean '.pyo'
    and '.pyc' from the users PATHEXT if it looks like it hit:
        http://bugs.activestate.com/ActivePython/show_bug.cgi?id=33311

    Usage:
        pythonw -E -S ca_cleanpathext.pyw

    Notes:
    - Only run with admin privs (because need to modify PATHEXT).
    - Call with "pythonw.exe" to avoid a DOS box.
    - Call with "-E" to ignore environment variables like PYTHONHOME
      that could screw up the Python invocation.
    - Call with "-S" to NOT import site.py, which could be hosed on the
      target machine if sys.path is bogus.
"""


#---- internal logging facility

class Logger:
    """Internal logging for this module.

    By default the logging threshold is WARN and message are logged to
    /dev/null (unless a filename or stream is passed to the constructor).
    """
    DEBUG, INFO, WARN, ERROR, FATAL = range(5)
    def __init__(self, threshold=None, streamOrFileName=None):
        import types
        if threshold is None:
            self.threshold = self.WARN
        else:
            self.threshold = threshold
        if type(streamOrFileName) == types.StringType:
            self.stream = open(streamOrFileName, 'w')
            self._opennedStream = 1
        else:
            self.stream = streamOrFileName
            self._opennedStream = 0
    def close(self):
        if self._opennedStream:
            self.stream.close()
    def _getLevelName(self, level):
        levelNameMap = {
            self.DEBUG: "DEBUG",
            self.INFO: "INFO",
            self.WARN: "WARN",
            self.ERROR: "ERROR",
            self.FATAL: "FATAL",
        }
        return levelNameMap[level]
    def log(self, level, msg, *args):
        if level < self.threshold:
            return
        message = "%-5s - " % self._getLevelName(level)
        if len(args):
            message += msg % args
        else:
            message += msg
        message += "\n"
        self.stream.write(message)
        self.stream.flush()
    def debug(self, msg, *args):
        self.log(self.DEBUG, msg, *args)
    def info(self, msg, *args):
        self.log(self.INFO, msg, *args)
    def warn(self, msg, *args):
        self.log(self.WARN, msg, *args)
    def error(self, msg, *args):
        self.log(self.ERROR, msg, *args)
    def fatal(self, msg, *args):
        self.log(self.FATAL, msg, *args)
    def exception(self, msg=None, *args):
        import traceback, cStringIO
        s = cStringIO.StringIO()
        exc_type, exc_value, exc_traceback = sys.exc_info()
        traceback.print_exception(exc_type, exc_value, exc_traceback,
                                  None, s)
        if msg:
            msg = msg + "\n" + s.getvalue()
        else:
            msg = s.getvalue()
        s.close()
        self.log(self.ERROR, msg, *args)
log = None



#---- support stuff

class Error(Exception):
    pass


def _rebuildSysPath():
    """Safely rebuild sys.path presuming that the current sys.path is
    completely hosed. This can happen is, say, the environment in which this
    script is run has PYTHONHOME set to a bogus value.
    """
    import sys
    assert sys.platform.startswith("win"), "Can only do this on Windows."

    installDir = sys.executable
    while installDir[-1] != '\\':
        installDir = installDir[:-1]
    installDir = installDir[:-1]

    sys.path = ["",
                installDir + "\\Lib\\site-packages\\pythonwin",
                installDir + "\\Lib\\site-packages\\win32",
                installDir + "\\Lib\\site-packages\\win32\\Lib",
                installDir + "\\Lib\\site-packages",
                installDir + "\\DLLs",
                installDir + "\\Lib",
                installDir + "\\Lib\\plat-win",
                installDir]




#---- mainline

def main(argv):
    """Clean .pyo;.pyc from PATHEXT."""
    log.info("sys.path = %s", sys.path)
    log.info("argv = %s", argv)

    # Info on tweaking system environment variables from:
    # http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/416087
    from _winreg import OpenKey, HKEY_LOCAL_MACHINE, QueryValueEx, \
                        SetValueEx, REG_SZ, KEY_ALL_ACCESS

    regpath = r'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'
    try:
        key = OpenKey(HKEY_LOCAL_MACHINE, regpath, 0, KEY_ALL_ACCESS)
    except WindowsError, ex:
        log.error("cannot find '%s' registry key (aborting): %s", regpath, ex)
        return 1

    try:
        pathext, type_id = QueryValueEx(key, "PATHEXT")
    except WindowsError, ex:
        log.info("cannot find 'PATHEXT' env var (done): %s", ex)
        return 0

    # The presence of ".pyo;.pyc;.pyw;.py" in PATHEXT is a very good
    # indicator of this bug. That's the one we will use.
    # Note: It has been considered to also also warn if not this pattern
    #       but .pyo or .pyc is on PATHEXT before .py? However this was
    #       dropped. It is *conceivable* that someone could set this up
    #       this way. It is not the purpose of this custom action to
    #       deal with this.
    indicator = ".pyo;.pyc;.pyw;.py"
    if indicator not in pathext:
        log.info("bug 33311 indicator, '%s', NOT found in PATHEXT: %r",
                 indicator, pathext)
        return 0
    log.info("bug 33311 indicator, '%s', FOUND in PATHEXT: %r",
             indicator, pathext)
    new_pathext = pathext.replace(indicator, ".py;.pyw")
    log.info("resetting PATHEXT to %r", new_pathext)

    try:
        SetValueEx(key, "PATHEXT", 0, REG_SZ, new_pathext)
    except WindowsError, ex:
        log.error("error setting new PATHEXT value: %s", ex)
        return 1

    # If have the requisite PyWin32 libs, then broadcast the environment
    # change. The broadcast is required (at least) to have the changes
    # take effect before a reboot. Jury is still out if even a reboot
    # works without this broadcast.
    log.info("broadcasting environment change to system")
    try:
        import win32gui, win32con
    except ImportError, ex:
        log.warn("could not broadcast environment change to system (%s): "
                 "will have to reboot for changes to take effect", ex)
        return 0
    try:
        win32gui.SendMessage(win32con.HWND_BROADCAST, win32con.WM_SETTINGCHANGE,
                             0, "Environment")
    except WindowsError, ex:
        log.warn("error broadcasting environment change to system: %s", ex)
        return 0

    return 0

if __name__ == '__main__':
    import sys
    _rebuildSysPath()

    # setup logging
    import tempfile, os
    scriptName = os.path.splitext(os.path.basename(sys.argv[0]))[0]
    logFileName = os.path.join(tempfile.gettempdir(), scriptName + ".log")
    log = Logger(Logger.DEBUG, logFileName)
    
    try:
        retval = main(sys.argv)
    except:
        log.exception()
        log.info("sys.path = %s", sys.path)
        log.close()
        raise
    else:
        log.close()

    sys.exit(retval)


