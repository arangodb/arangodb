# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""subprocess42 is the answer to life the universe and everything.

It has the particularity of having a Popen implementation that can yield output
as it is produced while implementing a timeout and NOT requiring the use of
worker threads.

Example:
  Wait for a child process with a timeout, send SIGTERM, wait a grace period
  then send SIGKILL:

    def wait_terminate_then_kill(proc, timeout, grace):
      try:
        return proc.wait(timeout)
      except subprocess42.TimeoutExpired:
        proc.terminate()
        try:
          return proc.wait(grace)
        except subprocess42.TimeoutExpired:
          proc.kill()
        return proc.wait()


TODO(maruel): Add VOID support like subprocess2.
"""

import contextlib
import errno
import os
import signal
import threading
import time

import subprocess

from subprocess import CalledProcessError, PIPE, STDOUT  # pylint: disable=W0611
from subprocess import list2cmdline


# Default maxsize argument.
MAX_SIZE = 16384


if subprocess.mswindows:
  import msvcrt  # pylint: disable=F0401
  from ctypes import wintypes
  from ctypes import windll


  # Which to be received depends on how this process was called and outside the
  # control of this script. See Popen docstring for more details.
  STOP_SIGNALS = (signal.SIGBREAK, signal.SIGTERM)


  def ReadFile(handle, desired_bytes):
    """Calls kernel32.ReadFile()."""
    c_read = wintypes.DWORD()
    buff = wintypes.create_string_buffer(desired_bytes+1)
    windll.kernel32.ReadFile(
        handle, buff, desired_bytes, wintypes.byref(c_read), None)
    # NULL terminate it.
    buff[c_read.value] = '\x00'
    return wintypes.GetLastError(), buff.value

  def PeekNamedPipe(handle):
    """Calls kernel32.PeekNamedPipe(). Simplified version."""
    c_avail = wintypes.DWORD()
    c_message = wintypes.DWORD()
    success = windll.kernel32.PeekNamedPipe(
        handle, None, 0, None, wintypes.byref(c_avail),
        wintypes.byref(c_message))
    if not success:
      raise OSError(wintypes.GetLastError())
    return c_avail.value

  def recv_multi_impl(conns, maxsize, timeout):
    """Reads from the first available pipe.

    It will immediately return on a closed connection, independent of timeout.

    Arguments:
    - maxsize: Maximum number of bytes to return. Defaults to MAX_SIZE.
    - timeout: If None, it is blocking. If 0 or above, will return None if no
          data is available within |timeout| seconds.

    Returns:
      tuple(int(index), str(data), bool(closed)).
    """
    assert conns
    assert timeout is None or isinstance(timeout, (int, float)), timeout
    maxsize = max(maxsize or MAX_SIZE, 1)

    # TODO(maruel): Use WaitForMultipleObjects(). Python creates anonymous pipes
    # for proc.stdout and proc.stderr but they are implemented as named pipes on
    # Windows. Since named pipes are not waitable object, they can't be passed
    # as-is to WFMO(). So this means N times CreateEvent(), N times ReadFile()
    # and finally WFMO(). This requires caching the events handles in the Popen
    # object and remembering the pending ReadFile() calls. This will require
    # some re-architecture to store the relevant event handle and OVERLAPPEDIO
    # object in Popen or the file object.
    start = time.time()
    handles = [
      (i, msvcrt.get_osfhandle(c.fileno())) for i, c in enumerate(conns)
    ]
    while True:
      for index, handle in handles:
        try:
          avail = min(PeekNamedPipe(handle), maxsize)
          if avail:
            return index, ReadFile(handle, avail)[1], False
        except OSError:
          # The pipe closed.
          return index, None, True

      if timeout is not None and (time.time() - start) >= timeout:
        return None, None, False
      # Polling rocks.
      time.sleep(0.001)

else:
  import fcntl  # pylint: disable=F0401
  import select


  # Signals that mean this process should exit quickly.
  STOP_SIGNALS = (signal.SIGINT, signal.SIGTERM)


  def recv_multi_impl(conns, maxsize, timeout):
    """Reads from the first available pipe.

    It will immediately return on a closed connection, independent of timeout.

    Arguments:
    - maxsize: Maximum number of bytes to return. Defaults to MAX_SIZE.
    - timeout: If None, it is blocking. If 0 or above, will return None if no
          data is available within |timeout| seconds.

    Returns:
      tuple(int(index), str(data), bool(closed)).
    """
    assert conns
    assert timeout is None or isinstance(timeout, (int, float)), timeout
    maxsize = max(maxsize or MAX_SIZE, 1)

    # select(timeout=0) will block, it has to be a value > 0.
    if timeout == 0:
      timeout = 0.001
    try:
      r, _, _ = select.select(conns, [], [], timeout)
    except select.error:
      r = None
    if not r:
      return None, None, False

    conn = r[0]
    # Temporarily make it non-blocking.
    # TODO(maruel): This is not very efficient when the caller is doing this in
    # a loop. Add a mechanism to have the caller handle this.
    flags = fcntl.fcntl(conn, fcntl.F_GETFL)
    if not conn.closed:
      # pylint: disable=E1101
      fcntl.fcntl(conn, fcntl.F_SETFL, flags | os.O_NONBLOCK)
    try:
      data = conn.read(maxsize)
      if not data:
        # On posix, this means the channel closed.
        return conns.index(conn), None, True
      return conns.index(conn), data, False
    finally:
      if not conn.closed:
        fcntl.fcntl(conn, fcntl.F_SETFL, flags)


class TimeoutExpired(Exception):
  """Compatible with python3 subprocess."""
  def __init__(self, cmd, timeout, output=None, stderr=None):
    self.cmd = cmd
    self.timeout = timeout
    self.output = output
    # Non-standard:
    self.stderr = stderr
    super(TimeoutExpired, self).__init__(str(self))

  def __str__(self):
    return "Command '%s' timed out after %s seconds" % (self.cmd, self.timeout)


class Popen(subprocess.Popen):
  """Adds timeout support on stdout and stderr.

  Inspired by
  http://code.activestate.com/recipes/440554-module-to-allow-asynchronous-subprocess-use-on-win/

  Unlike subprocess, yield_any(), recv_*(), communicate() will close stdout and
  stderr once the child process closes them, after all the data is read.

  Arguments:
  - detached: If True, the process is created in a new process group. On
    Windows, use CREATE_NEW_PROCESS_GROUP. On posix, use os.setpgid(0, 0).

  Additional members:
  - start: timestamp when this process started.
  - end: timestamp when this process exited, as seen by this process.
  - detached: If True, the child process was started as a detached process.
  - gid: process group id, if any.
  - duration: time in seconds the process lasted.

  Additional methods:
  - yield_any(): yields output until the process terminates.
  - recv_any(): reads from stdout and/or stderr with optional timeout.
  - recv_out() & recv_err(): specialized version of recv_any().
  """
  # subprocess.Popen.__init__() is not threadsafe; there is a race between
  # creating the exec-error pipe for the child and setting it to CLOEXEC during
  # which another thread can fork and cause the pipe to be inherited by its
  # descendents, which will cause the current Popen to hang until all those
  # descendents exit. Protect this with a lock so that only one fork/exec can
  # happen at a time.
  popen_lock = threading.Lock()

  def __init__(self, args, **kwargs):
    assert 'creationflags' not in kwargs
    assert 'preexec_fn' not in kwargs, 'Use detached=True instead'
    self.start = time.time()
    self.end = None
    self.gid = None
    self.detached = kwargs.pop('detached', False)
    if self.detached:
      if subprocess.mswindows:
        kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP
      else:
        kwargs['preexec_fn'] = lambda: os.setpgid(0, 0)
    with self.popen_lock:
      super(Popen, self).__init__(args, **kwargs)
    self.args = args
    if self.detached and not subprocess.mswindows:
      self.gid = os.getpgid(self.pid)

  def duration(self):
    """Duration of the child process.

    It is greater or equal to the actual time the child process ran. It can be
    significantly higher than the real value if neither .wait() nor .poll() was
    used.
    """
    return (self.end or time.time()) - self.start

  # pylint: disable=arguments-differ,redefined-builtin
  def communicate(self, input=None, timeout=None):
    """Implements python3's timeout support.

    Unlike wait(), timeout=0 is considered the same as None.

    Raises:
    - TimeoutExpired when more than timeout seconds were spent waiting for the
      process.
    """
    if not timeout:
      return super(Popen, self).communicate(input=input)

    assert isinstance(timeout, (int, float)), timeout

    if self.stdin or self.stdout or self.stderr:
      stdout = '' if self.stdout else None
      stderr = '' if self.stderr else None
      t = None
      if input is not None:
        assert self.stdin, (
            'Can\'t use communicate(input) if not using '
            'Popen(stdin=subprocess42.PIPE')
        # TODO(maruel): Switch back to non-threading.
        def write():
          try:
            self.stdin.write(input)
          except IOError:
            pass
        t = threading.Thread(name='Popen.communicate', target=write)
        t.daemon = True
        t.start()

      try:
        if self.stdout or self.stderr:
          start = time.time()
          end = start + timeout
          def remaining():
            return max(end - time.time(), 0)
          for pipe, data in self.yield_any(timeout=remaining):
            if pipe is None:
              raise TimeoutExpired(self.args, timeout, stdout, stderr)
            assert pipe in ('stdout', 'stderr'), pipe
            if pipe == 'stdout':
              stdout += data
            else:
              stderr += data
        else:
          # Only stdin is piped.
          self.wait(timeout=timeout)
      finally:
        if t:
          try:
            self.stdin.close()
          except IOError:
            pass
          t.join()
    else:
      # No pipe. The user wanted to use wait().
      self.wait(timeout=timeout)
      return None, None

    # Indirectly initialize self.end.
    self.wait()
    return stdout, stderr

  def wait(self, timeout=None):  # pylint: disable=arguments-differ
    """Implements python3's timeout support.

    Raises:
    - TimeoutExpired when more than timeout seconds were spent waiting for the
      process.
    """
    assert timeout is None or isinstance(timeout, (int, float)), timeout
    if timeout is None:
      super(Popen, self).wait()
    elif self.returncode is None:
      if subprocess.mswindows:
        WAIT_TIMEOUT = 258
        result = subprocess._subprocess.WaitForSingleObject(
            self._handle, int(timeout * 1000))
        if result == WAIT_TIMEOUT:
          raise TimeoutExpired(self.args, timeout)
        self.returncode = subprocess._subprocess.GetExitCodeProcess(
            self._handle)
      else:
        # If you think the following code is horrible, it's because it is
        # inspired by python3's stdlib.
        end = time.time() + timeout
        delay = 0.001
        while True:
          try:
            pid, sts = subprocess._eintr_retry_call(
                os.waitpid, self.pid, os.WNOHANG)
          except OSError as e:
            if e.errno != errno.ECHILD:
              raise
            pid = self.pid
            sts = 0
          if pid == self.pid:
            # This sets self.returncode.
            self._handle_exitstatus(sts)
            break
          remaining = end - time.time()
          if remaining <= 0:
            raise TimeoutExpired(self.args, timeout)
          delay = min(delay * 2, remaining, .05)
          time.sleep(delay)

    if not self.end:
      # communicate() uses wait() internally.
      self.end = time.time()
    return self.returncode

  def poll(self):
    ret = super(Popen, self).poll()
    if ret is not None and not self.end:
      self.end = time.time()
    return ret

  def yield_any(self, maxsize=None, timeout=None):
    """Yields output until the process terminates.

    Unlike wait(), does not raise TimeoutExpired.

    Yields:
      (pipename, data) where pipename is either 'stdout', 'stderr' or None in
      case of timeout or when the child process closed one of the pipe(s) and
      all pending data on the pipe was read.

    Arguments:
    - maxsize: See recv_any(). Can be a callable function.
    - timeout: If None, the call is blocking. If set, yields None, None if no
          data is available within |timeout| seconds. It resets itself after
          each yield. Can be a callable function.
    """
    assert self.stdout or self.stderr
    if timeout is not None:
      # timeout=0 effectively means that the pipe is continuously polled.
      if isinstance(timeout, (int, float)):
        assert timeout >= 0, timeout
        old_timeout = timeout
        timeout = lambda: old_timeout
      else:
        assert callable(timeout), timeout

    if maxsize is not None and not callable(maxsize):
      assert isinstance(maxsize, (int, float)), maxsize

    last_yield = time.time()
    while self.poll() is None:
      to = (None if timeout is None
            else max(timeout() - (time.time() - last_yield), 0))
      t, data = self.recv_any(
          maxsize=maxsize() if callable(maxsize) else maxsize, timeout=to)
      if data or to is 0:
        yield t, data
        last_yield = time.time()

    # Read all remaining output in the pipes.
    # There is 3 cases:
    # - pipes get closed automatically by the calling process before it exits
    # - pipes are closed automated by the OS
    # - pipes are kept open due to grand-children processes outliving the
    #   children process.
    while True:
      ms = maxsize
      if callable(maxsize):
        ms = maxsize()
      # timeout=0 is mainly to handle the case where a grand-children process
      # outlives the process started.
      t, data = self.recv_any(maxsize=ms, timeout=0)
      if not data:
        break
      yield t, data

  def recv_any(self, maxsize=None, timeout=None):
    """Reads from the first pipe available from stdout and stderr.

    Unlike wait(), it does not throw TimeoutExpired.

    Arguments:
    - maxsize: Maximum number of bytes to return. Defaults to MAX_SIZE.
    - timeout: If None, it is blocking. If 0 or above, will return None if no
          data is available within |timeout| seconds.

    Returns:
      tuple(pipename or None, str(data)). pipename is one of 'stdout' or
      'stderr'.
    """
    # recv_multi_impl will early exit on a closed connection. Loop accordingly
    # to simplify call sites.
    while True:
      pipes = [
        x for x in ((self.stderr, 'stderr'), (self.stdout, 'stdout')) if x[0]
      ]
      # If both stdout and stderr have the exact file handle, they are
      # effectively the same pipe. Deduplicate it since otherwise it confuses
      # recv_multi_impl().
      if len(pipes) == 2 and self.stderr.fileno() == self.stdout.fileno():
        pipes.pop(0)

      if not pipes:
        return None, None
      start = time.time()
      conns, names = zip(*pipes)
      index, data, closed = recv_multi_impl(conns, maxsize, timeout)
      if index is None:
        return index, data
      if closed:
        self._close(names[index])
        if not data:
          # Loop again. The other pipe may still be open.
          if timeout:
            timeout -= (time.time() - start)
          continue

      if self.universal_newlines:
        data = self._translate_newlines(data)
      return names[index], data

  def recv_out(self, maxsize=None, timeout=None):
    """Reads from stdout synchronously with timeout."""
    return self._recv('stdout', maxsize, timeout)

  def recv_err(self, maxsize=None, timeout=None):
    """Reads from stderr synchronously with timeout."""
    return self._recv('stderr', maxsize, timeout)

  def terminate(self):
    """Tries to do something saner on Windows that the stdlib.

    Windows:
      self.detached/CREATE_NEW_PROCESS_GROUP determines what can be used:
      - If set, only SIGBREAK can be sent and it is sent to a single process.
      - If not set, in theory only SIGINT can be used and *all processes* in
         the processgroup receive it. In practice, we just kill the process.
      See http://msdn.microsoft.com/library/windows/desktop/ms683155.aspx
      The default on Windows is to call TerminateProcess() always, which is not
      useful.

    On Posix, always send SIGTERM.
    """
    try:
      if subprocess.mswindows and self.detached:
        return self.send_signal(signal.CTRL_BREAK_EVENT)
      super(Popen, self).terminate()
    except OSError:
      # The function will throw if the process terminated in-between. Swallow
      # this.
      pass

  def kill(self):
    """Kills the process and its children if possible.

    Swallows exceptions and return True on success.
    """
    if self.gid:
      try:
        os.killpg(self.gid, signal.SIGKILL)
      except OSError:
        return False
    else:
      try:
        super(Popen, self).kill()
      except OSError:
        return False
    return True

  def _close(self, which):
    """Closes either stdout or stderr."""
    getattr(self, which).close()
    setattr(self, which, None)

  def _recv(self, which, maxsize, timeout):
    """Reads from one of stdout or stderr synchronously with timeout."""
    conn = getattr(self, which)
    if conn is None:
      return None
    _, data, closed = recv_multi_impl([conn], maxsize, timeout)
    if closed:
      self._close(which)
    if self.universal_newlines and data:
      data = self._translate_newlines(data)
    return data


@contextlib.contextmanager
def set_signal_handler(signals, handler):
  """Temporarilly override signals handler.

  Useful when waiting for a child process to handle signals like SIGTERM, so the
  signal can be propagated to the child process.
  """
  previous = {s: signal.signal(s, handler) for s in signals}
  try:
    yield
  finally:
    for sig, h in previous.iteritems():
      signal.signal(sig, h)


def call(*args, **kwargs):
  """Adds support for timeout."""
  timeout = kwargs.pop('timeout', None)
  return Popen(*args, **kwargs).wait(timeout)


def check_call(*args, **kwargs):
  """Adds support for timeout."""
  retcode = call(*args, **kwargs)
  if retcode:
    raise CalledProcessError(retcode, kwargs.get('args') or args[0])
  return 0


def check_output(*args, **kwargs):
  """Adds support for timeout."""
  timeout = kwargs.pop('timeout', None)
  if 'stdout' in kwargs:
    raise ValueError('stdout argument not allowed, it will be overridden.')
  process = Popen(stdout=PIPE, *args, **kwargs)
  output, _ = process.communicate(timeout=timeout)
  retcode = process.poll()
  if retcode:
    raise CalledProcessError(retcode, kwargs.get('args') or args[0], output)
  return output


def call_with_timeout(args, timeout, **kwargs):
  """Runs an executable; kill it in case of timeout."""
  proc = Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, **kwargs)
  try:
    out, err = proc.communicate(timeout=timeout)
  except TimeoutExpired as e:
    out = e.output
    err = e.stderr
    proc.kill()
    proc.wait()
  return out, err, proc.returncode, proc.duration()
