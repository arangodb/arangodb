# Common py2exe boot script - executed for all target types.

# When we are a windows_exe we have no console, and writing to
# sys.stderr or sys.stdout will sooner or later raise an exception,
# and tracebacks will be lost anyway (see explanation below).
#
# We assume that output to sys.stdout can go to the bitsink, but we
# *want* to see tracebacks.  So we redirect sys.stdout into an object
# with a write method doing nothing, and sys.stderr into a logfile
# having the same name as the executable, with '.log' appended.
#
# We only open the logfile if something is written to sys.stderr.
#
# If the logfile cannot be opened for *any* reason, we have no choice
# but silently ignore the error.
#
# It remains to be seen if the 'a' flag for opening the logfile is a
# good choice, or 'w' would be better.
#
# More elaborate explanation on why this is needed:
#
# The sys.stdout and sys.stderr that GUI programs get (from Windows) are
# more than useless.  This is not a py2exe problem, pythonw.exe behaves
# in the same way.
#
# To demonstrate, run this program with pythonw.exe:
#
# import sys
# sys.stderr = open("out.log", "w")
# for i in range(10000):
#     print i
#
# and open the 'out.log' file.  It contains this:
#
# Traceback (most recent call last):
#   File "out.py", line 6, in ?
#     print i
# IOError: [Errno 9] Bad file descriptor
#
# In other words, after printing a certain number of bytes to the
# system-supplied sys.stdout (or sys.stderr) an exception will be raised.
#

import sys
if sys.frozen == "windows_exe":
    class Stderr(object):
        softspace = 0
        _file = None
        _error = None
        def write(self, text, alert=sys._MessageBox, fname=sys.executable + '.log'):
            if self._file is None and self._error is None:
                try:
                    self._file = open(fname, 'a')
                except Exception, details:
                    self._error = details
                    import atexit
                    atexit.register(alert, 0,
                                    "The logfile '%s' could not be opened:\n %s" % \
                                    (fname, details),
                                    "Errors occurred")
                else:
                    import atexit
                    atexit.register(alert, 0,
                                    "See the logfile '%s' for details" % fname,
                                    "Errors occurred")
            if self._file is not None:
                self._file.write(text)
                self._file.flush()
        def flush(self):
            if self._file is not None:
                self._file.flush()
    sys.stderr = Stderr()
    del sys._MessageBox
    del Stderr

    class Blackhole(object):
        softspace = 0
        def write(self, text):
            pass
        def flush(self):
            pass
    sys.stdout = Blackhole()
    del Blackhole
del sys

# Disable linecache.getline() which is called by
# traceback.extract_stack() when an exception occurs to try and read
# the filenames embedded in the packaged python code.  This is really
# annoying on windows when the d: or e: on our build box refers to
# someone elses removable or network drive so the getline() call
# causes it to ask them to insert a disk in that drive.
import linecache
def fake_getline(filename, lineno, module_globals=None):
    return ''
linecache.orig_getline = linecache.getline
linecache.getline = fake_getline

del linecache, fake_getline
