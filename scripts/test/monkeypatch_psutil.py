#!/usr/bin/env python
"""
This file is intended to make psutil behave portable on the wintendo, and
to enable functionality the upstream author doesn't think ready to use.
"""
import platform
import signal
import subprocess
import time
# import sys

winver = platform.win32_ver()
if winver[0]:
    WINDOWS = True
    POSIX = False
    # may throw on elderly wintendos?
    # this is only here on the wintendo, don't bother me else where.
    # pylint: disable=no-name-in-module
    from psutil import Process
    from psutil import TimeoutExpired
    from psutil import _psutil_windows as cext

    from psutil._pswindows import WindowsService

    class WindowsServiceMonkey(WindowsService):
        """
        We want this code anyways, its good enough!
        actions
        XXX: the necessary C bindings for start() and stop() are
        implemented but for now I prefer not to expose them.
        I may change my mind in the future. Reasons:
        - they require Administrator privileges
        - can't implement a timeout for stop() (unless by using a thread,
          which sucks)
        - would require adding ServiceAlreadyStarted and
          ServiceAlreadyStopped exceptions, adding two new APIs.
        - we might also want to have modify(), which would basically mean
          rewriting win32serviceutil.ChangeServiceConfig, which involves a
          lot of stuff (and API constants which would pollute the API), see:
          http://pyxr.sourceforge.net/PyXR/c/python24/lib/site-packages/
              win32/lib/win32serviceutil.py.html#0175
        - psutil is typically about "read only" monitoring stuff;
          win_service_* APIs should only be used to retrieve a service and
          check whether it's running
        """

        def start(self, timeout=None):
            """start a windows service"""
            with self._wrap_exceptions():
                cext.winservice_start(self.name())
                if timeout:
                    giveup_at = time.time() + timeout
                    while True:
                        if self.status() == "running":
                            return
                        if time.time() > giveup_at:
                            raise TimeoutExpired(timeout)
                        time.sleep(0.1)

        def stop(self):
            """stop windows service"""
            # Note: timeout is not implemented because it's just not
            # possible, see:
            # http://stackoverflow.com/questions/11973228/
            with self._wrap_exceptions():
                return cext.winservice_stop(self.name())

    WindowsService.start = WindowsServiceMonkey.start
    WindowsService.stop = WindowsServiceMonkey.stop

    class ProcessMonkey(Process):
        """overload this function"""

        def terminate(self):
            """Terminate the process with SIGTERM pre-emptively checking
            whether PID has been reused.
            On Windows this will only work for processes spawned through psutil
            or started using
               kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP.
            """
            if POSIX:
                self._send_signal(signal.SIGTERM)
            else:  # pragma: no cover

                # def sigint_boomerang_handler(signum, frame):
                #     """do the right thing to behave like linux does"""
                #     # pylint: disable=unused-argument
                #     if signum != signal.SIGINT:
                #         sys.exit(1)
                #     # pylint: disable=unnecessary-pass
                #     pass
                #
                # original_sigint_handler = signal.getsignal(signal.SIGINT)
                # signal.signal(signal.SIGINT, sigint_boomerang_handler)
                # # only here on the wintendo:
                # # pylint: disable=no-member
                # self.send_signal(signal.CTRL_BREAK_EVENT)
                self.wait()
                # restore original handler
                # signal.signal(signal.SIGINT, original_sigint_handler)

    Process.terminate = ProcessMonkey.terminate
    # pylint: disable=super-init-not-called disable=consider-using-with
    class Popen(Process):
        """overload this function"""

        def __init__(self, *args, **kwargs):
            kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
            self.__subproc = subprocess.Popen(*args, **kwargs)
            self._init(self.__subproc.pid, _ignore_nsp=True)

    from psutil import Popen as patchme_popen

    patchme_popen.__init__ = Popen.__init__
