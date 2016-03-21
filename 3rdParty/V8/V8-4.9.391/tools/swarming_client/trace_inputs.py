#!/usr/bin/env python
# coding=utf-8
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Traces an executable and its child processes and extract the files accessed
by them.

The implementation uses OS-specific API. The native Kernel logger and the ETL
interface is used on Windows. Dtrace is used on OSX. Strace is used otherwise.
The OS-specific implementation is hidden in an 'API' interface.

The results are embedded in a Results instance. The tracing is done in two
phases, the first is to do the actual trace and generate an
implementation-specific log file. Then the log file is parsed to extract the
information, including the individual child processes and the files accessed
from the log.
"""

import codecs
import csv
import errno
import getpass
import glob
import logging
import os
import re
import stat
import subprocess
import sys
import tempfile
import threading
import time
import weakref

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import file_path
from utils import fs
from utils import logging_utils
from utils import tools

## OS-specific imports

if sys.platform == 'win32':
  from ctypes.wintypes import byref, c_int, c_wchar_p
  from ctypes.wintypes import windll # pylint: disable=E0611


__version__ = '0.2'


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


class TracingFailure(Exception):
  """An exception occured during tracing."""
  def __init__(self, description, pid, line_number, line, *args):
    super(TracingFailure, self).__init__(
        description, pid, line_number, line, *args)
    self.description = description
    self.pid = pid
    self.line_number = line_number
    self.line = line
    self.extra = args

  def __str__(self):
    out = self.description
    if self.pid:
      out += '\npid: %d' % self.pid
    if self.line_number:
      out += '\nline: %d' % self.line_number
    if self.line:
      out += '\n%s' % self.line
    if self.extra:
      out += '\n' + ', '.join(map(str, filter(None, self.extra)))
    return out


## OS-specific functions

if sys.platform == 'win32':
  def get_current_encoding():
    """Returns the 'ANSI' code page associated to the process."""
    return 'cp%d' % int(windll.kernel32.GetACP())


  def CommandLineToArgvW(command_line):
    """Splits a commandline into argv using CommandLineToArgvW()."""
    # http://msdn.microsoft.com/library/windows/desktop/bb776391.aspx
    size = c_int()
    assert isinstance(command_line, unicode)
    ptr = windll.shell32.CommandLineToArgvW(command_line, byref(size))
    try:
      return [arg for arg in (c_wchar_p * size.value).from_address(ptr)]
    finally:
      windll.kernel32.LocalFree(ptr)


def can_trace():
  """Returns True if the user is an administrator on Windows.

  It is required for tracing to work.
  """
  if sys.platform != 'win32':
    return True
  return bool(windll.shell32.IsUserAnAdmin())


def create_subprocess_thunk():
  """Creates a small temporary script to start the child process.

  This thunk doesn't block, its unique name is used to identify it as the
  parent.
  """
  handle, name = tempfile.mkstemp(prefix=u'trace_inputs_thunk', suffix='.py')
  try:
    os.write(
        handle,
        (
          'import subprocess, sys\n'
          'sys.exit(subprocess.call(sys.argv[2:]))\n'
        ))
  finally:
    os.close(handle)
  return name


def create_exec_thunk():
  """Creates a small temporary script to start the child executable.

  Reads from the file handle provided as the fisrt argument to block, then
  execv() the command to be traced.
  """
  handle, name = tempfile.mkstemp(prefix=u'trace_inputs_thunk', suffix='.py')
  try:
    os.write(
        handle,
        (
          'import os, sys\n'
          'fd = int(sys.argv[1])\n'
          # This will block until the controlling process writes a byte on the
          # pipe. It will do so once the tracing tool, e.g. strace, is ready to
          # trace.
          'os.read(fd, 1)\n'
          'os.close(fd)\n'
          'os.execv(sys.argv[2], sys.argv[2:])\n'
        ))
  finally:
    os.close(handle)
  return name


def strace_process_quoted_arguments(text):
  """Extracts quoted arguments on a string and return the arguments as a list.

  Implemented as an automaton. Supports incomplete strings in the form
  '"foo"...'.

  Example:
    With text = '"foo", "bar"', the function will return ['foo', 'bar']

  TODO(maruel): Implement escaping.
  """
  # All the possible states of the DFA.
  ( NEED_QUOTE,         # Begining of a new arguments.
    INSIDE_STRING,      # Inside an argument.
    ESCAPED,            # Found a '\' inside a quote. Treat the next char as-is.
    NEED_COMMA_OR_DOT,  # Right after the closing quote of an argument. Could be
                        # a serie of 3 dots or a comma.
    NEED_SPACE,         # Right after a comma
    NEED_DOT_2,         # Found a dot, need a second one.
    NEED_DOT_3,         # Found second dot, need a third one.
    NEED_COMMA,         # Found third dot, need a comma.
    ) = range(8)

  state = NEED_QUOTE
  out = []
  for index, char in enumerate(text):
    if char == '"':
      if state == NEED_QUOTE:
        state = INSIDE_STRING
        # A new argument was found.
        out.append('')
      elif state == INSIDE_STRING:
        # The argument is now closed.
        state = NEED_COMMA_OR_DOT
      elif state == ESCAPED:
        out[-1] += char
        state = INSIDE_STRING
      else:
        raise ValueError(
            'Can\'t process char \'%s\' at column %d for: %r' % (
              char, index, text),
            index,
            text)
    elif char == ',':
      if state in (NEED_COMMA_OR_DOT, NEED_COMMA):
        state = NEED_SPACE
      elif state == INSIDE_STRING:
        out[-1] += char
      elif state == ESCAPED:
        out[-1] += char
        state = INSIDE_STRING
      else:
        raise ValueError(
            'Can\'t process char \'%s\' at column %d for: %r' % (
              char, index, text),
            index,
            text)
    elif char == ' ':
      if state == NEED_SPACE:
        state = NEED_QUOTE
      elif state == INSIDE_STRING:
        out[-1] += char
      elif state == ESCAPED:
        out[-1] += char
        state = INSIDE_STRING
      else:
        raise ValueError(
            'Can\'t process char \'%s\' at column %d for: %r' % (
              char, index, text),
            index,
            text)
    elif char == '.':
      if state in (NEED_QUOTE, NEED_COMMA_OR_DOT):
        # The string is incomplete, this mean the strace -s flag should be
        # increased.
        # For NEED_QUOTE, the input string would look like '"foo", ...'.
        # For NEED_COMMA_OR_DOT, the input string would look like '"foo"...'
        state = NEED_DOT_2
      elif state == NEED_DOT_2:
        state = NEED_DOT_3
      elif state == NEED_DOT_3:
        state = NEED_COMMA
      elif state == INSIDE_STRING:
        out[-1] += char
      elif state == ESCAPED:
        out[-1] += char
        state = INSIDE_STRING
      else:
        raise ValueError(
            'Can\'t process char \'%s\' at column %d for: %r' % (
              char, index, text),
            index,
            text)
    elif char == '\\':
      if state == ESCAPED:
        out[-1] += char
        state = INSIDE_STRING
      elif state == INSIDE_STRING:
        state = ESCAPED
      else:
        raise ValueError(
            'Can\'t process char \'%s\' at column %d for: %r' % (
              char, index, text),
            index,
            text)
    else:
      if state == INSIDE_STRING:
        out[-1] += char
      else:
        raise ValueError(
            'Can\'t process char \'%s\' at column %d for: %r' % (
              char, index, text),
            index,
            text)
  if state not in (NEED_COMMA, NEED_COMMA_OR_DOT):
    raise ValueError(
        'String is incorrectly terminated: %r' % text,
        text)
  return out


def assert_is_renderable(pseudo_string):
  """Asserts the input is a valid object to be processed by render()."""
  assert (
      pseudo_string is None or
      isinstance(pseudo_string, unicode) or
      hasattr(pseudo_string, 'render')), repr(pseudo_string)


def render(pseudo_string):
  """Converts the pseudo-string to an unicode string."""
  if pseudo_string is None or isinstance(pseudo_string, unicode):
    return pseudo_string
  return pseudo_string.render()


class Results(object):
  """Results of a trace session."""

  class _TouchedObject(object):
    """Something, a file or a directory, that was accessed."""
    def __init__(self, root, path, tainted, size, nb_files):
      logging.debug(
          '%s(%s, %s, %s, %s, %s)' %
          (self.__class__.__name__, root, path, tainted, size, nb_files))
      assert_is_renderable(root)
      assert_is_renderable(path)
      self.root = root
      self.path = path
      self.tainted = tainted
      self.nb_files = nb_files
      # Can be used as a cache or a default value, depending on context. In
      # particular, once self.tainted is True, because the path was replaced
      # with a variable, it is not possible to look up the file size.
      self._size = size
      # These are cache only.
      self._real_path = None

      # Check internal consistency.
      assert path, path
      assert tainted or bool(root) != bool(file_path.isabs(path)), (root, path)
      assert tainted or (
          not fs.exists(self.full_path) or
          (self.full_path == file_path.get_native_path_case(self.full_path))), (
              tainted,
              self.full_path,
              file_path.get_native_path_case(self.full_path))

    @property
    def existent(self):
      return self.size != -1

    @property
    def full_path(self):
      if self.root:
        return os.path.join(self.root, self.path)
      return self.path

    @property
    def real_path(self):
      """Returns the path with symlinks resolved."""
      if not self._real_path:
        self._real_path = fs.path.realpath(self.full_path)
      return self._real_path

    @property
    def size(self):
      """File's size. -1 is not existent.

      Once tainted, it is not possible the retrieve the file size anymore since
      the path is composed of variables.
      """
      if self._size is None and not self.tainted:
        try:
          self._size = fs.stat(self.full_path).st_size
        except OSError:
          self._size = -1
      return self._size

    def flatten(self):
      """Returns a dict representing this object.

      A 'size' of 0 means the file was only touched and not read.
      """
      return {
        'path': self.path,
        'size': self.size,
      }

    def replace_variables(self, variables):
      """Replaces the root of this File with one of the variables if it matches.

      If a variable replacement occurs, the cloned object becomes tainted.
      """
      for variable, root_path in variables.iteritems():
        if self.path.startswith(root_path):
          return self._clone(
              self.root, variable + self.path[len(root_path):], True)
      # No need to clone, returns ourself.
      return self

    def strip_root(self, root):
      """Returns a clone of itself with 'root' stripped off.

      Note that the file is kept if it is either accessible from a symlinked
      path that was used to access the file or through the real path.
      """
      # Check internal consistency.
      assert (
          self.tainted or
          (file_path.isabs(root) and root.endswith(os.path.sep))), root
      if not self.full_path.startswith(root):
        # Now try to resolve the symlinks to see if it can be reached this way.
        # Only try *after* trying without resolving symlink.
        if not self.real_path.startswith(root):
          return None
        path = self.real_path
      else:
        path = self.full_path
      return self._clone(root, path[len(root):], self.tainted)

    def _clone(self, new_root, new_path, tainted):
      raise NotImplementedError(self.__class__.__name__)

  class File(_TouchedObject):
    """A file that was accessed. May not be present anymore.

    If tainted is true, it means it is not a real path anymore as a variable
    replacement occured.

    |mode| can be one of None, TOUCHED, READ or WRITE.
    """
    # Was probed for existence, and it is existent, but was never _opened_.
    TOUCHED = 't'
    # Opened for read only and guaranteed to not have been written to.
    READ = 'r'
    # Opened for write.
    WRITE = 'w'

    # They are listed in order of priority. E.g. if a file is traced as TOUCHED
    # then as WRITE, only keep WRITE. None means no idea, which is a problem on
    # Windows.
    ACCEPTABLE_MODES = (None, TOUCHED, READ, WRITE)

    def __init__(self, root, path, tainted, size, mode):
      assert mode in self.ACCEPTABLE_MODES
      super(Results.File, self).__init__(root, path, tainted, size, 1)
      self.mode = mode

    def _clone(self, new_root, new_path, tainted):
      """Clones itself keeping meta-data."""
      # Keep the self.size and self._real_path caches for performance reason. It
      # is also important when the file becomes tainted (with a variable instead
      # of the real path) since self.path is not an on-disk path anymore so
      # out._size cannot be updated.
      out = self.__class__(new_root, new_path, tainted, self.size, self.mode)
      out._real_path = self._real_path
      return out

    def flatten(self):
      out = super(Results.File, self).flatten()
      out['mode'] = self.mode
      return out

  class Directory(_TouchedObject):
    """A directory of files. Must exist.

    For a Directory instance, self.size is not a cache, it's an actual value
    that is never modified and represents the total size of the files contained
    in this directory. It is possible that the directory is empty so that
    size==0; this happens if there's only an invalid symlink in it.
    """
    def __init__(self, root, path, tainted, size, nb_files):
      """path='.' is a valid value and must be handled appropriately."""
      assert not path.endswith(os.path.sep), path
      super(Results.Directory, self).__init__(
          root, path + os.path.sep, tainted, size, nb_files)

    def flatten(self):
      out = super(Results.Directory, self).flatten()
      out['nb_files'] = self.nb_files
      return out

    def _clone(self, new_root, new_path, tainted):
      """Clones itself keeping meta-data."""
      out = self.__class__(
          new_root,
          new_path.rstrip(os.path.sep),
          tainted,
          self.size,
          self.nb_files)
      out._real_path = self._real_path
      return out

  class Process(object):
    """A process that was traced.

    Contains references to the files accessed by this process and its children.
    """
    def __init__(self, pid, files, executable, command, initial_cwd, children):
      logging.debug('Process(%s, %d, ...)' % (pid, len(files)))
      self.pid = pid
      self.files = sorted(files, key=lambda x: x.path)
      self.children = children
      self.executable = executable
      self.command = command
      self.initial_cwd = initial_cwd

      # Check internal consistency.
      assert len(set(f.path for f in self.files)) == len(self.files), sorted(
          f.path for f in self.files)
      assert isinstance(self.children, list)
      assert isinstance(self.files, list)

    @property
    def all(self):
      for child in self.children:
        for i in child.all:
          yield i
      yield self

    def flatten(self):
      return {
        'children': [c.flatten() for c in self.children],
        'command': self.command,
        'executable': self.executable,
        'files': [f.flatten() for f in self.files],
        'initial_cwd': self.initial_cwd,
        'pid': self.pid,
      }

    def strip_root(self, root):
      assert file_path.isabs(root) and root.endswith(os.path.sep), root
      # Loads the files after since they are constructed as objects.
      out = self.__class__(
          self.pid,
          filter(None, (f.strip_root(root) for f in self.files)),
          self.executable,
          self.command,
          self.initial_cwd,
          [c.strip_root(root) for c in self.children])
      logging.debug(
          'strip_root(%s) %d -> %d' % (root, len(self.files), len(out.files)))
      return out

  def __init__(self, process):
    self.process = process
    # Cache.
    self._files = None

  def flatten(self):
    return {
      'root': self.process.flatten(),
    }

  @property
  def files(self):
    if self._files is None:
      self._files = sorted(
          sum((p.files for p in self.process.all), []),
          key=lambda x: x.path)
    return self._files

  @property
  def existent(self):
    return [f for f in self.files if f.existent]

  @property
  def non_existent(self):
    return [f for f in self.files if not f.existent]

  def strip_root(self, root):
    """Returns a clone with all the files outside the directory |root| removed
    and converts all the path to be relative paths.

    It keeps files accessible through the |root| directory or that have been
    accessed through any symlink which points to the same directory.
    """
    # Resolve any symlink
    root = fs.realpath(root)
    root = (
        file_path.get_native_path_case(root).rstrip(os.path.sep) + os.path.sep)
    logging.debug('strip_root(%s)' % root)
    return Results(self.process.strip_root(root))


class ApiBase(object):
  """OS-agnostic API to trace a process and its children."""
  class Context(object):
    """Processes one log line at a time and keeps the list of traced processes.

    The parsing is complicated by the fact that logs are traced out of order for
    strace but in-order for dtrace and logman. In addition, on Windows it is
    very frequent that processids are reused so a flat list cannot be used. But
    at the same time, it is impossible to faithfully construct a graph when the
    logs are processed out of order. So both a tree and a flat mapping are used,
    the tree is the real process tree, while the flat mapping stores the last
    valid process for the corresponding processid. For the strace case, the
    tree's head is guessed at the last moment.
    """
    class Process(object):
      """Keeps context for one traced child process.

      Logs all the files this process touched. Ignores directories.
      """
      def __init__(self, blacklist, pid, initial_cwd):
        # Check internal consistency.
        assert isinstance(pid, int), repr(pid)
        assert_is_renderable(initial_cwd)
        self.pid = pid
        # children are Process instances.
        self.children = []
        self.initial_cwd = initial_cwd
        self.cwd = None
        self.files = {}
        self.executable = None
        self.command = None
        self._blacklist = blacklist

      def to_results_process(self):
        """Resolves file case sensitivity and or late-bound strings."""
        # When resolving files, it's normal to get dupe because a file could be
        # opened multiple times with different case. Resolve the deduplication
        # here.
        def fix_path(x):
          """Returns the native file path case.

          Converts late-bound strings.
          """
          if not x:
            # Do not convert None instance to 'None'.
            return x
          x = render(x)
          if os.path.isabs(x):
            # If the path is not absolute, which tends to happen occasionally on
            # Windows, it is not possible to get the native path case so ignore
            # that trace. It mostly happens for 'executable' value.
            x = file_path.get_native_path_case(x)
          return x

        def fix_and_blacklist_path(x, m):
          """Receives a tuple (filepath, mode) and processes filepath."""
          x = fix_path(x)
          if not x:
            return
          # The blacklist needs to be reapplied, since path casing could
          # influence blacklisting.
          if self._blacklist(x):
            return
          # Filters out directories. Some may have passed through.
          if fs.isdir(x):
            return
          return x, m

        # Renders all the files as strings, as some could be RelativePath
        # instances. It is important to do it first since there could still be
        # multiple entries with the same path but different modes.
        rendered = (
            fix_and_blacklist_path(f, m) for f, m in self.files.iteritems())
        files = sorted(
          (f for f in rendered if f),
          key=lambda x: (x[0], Results.File.ACCEPTABLE_MODES.index(x[1])))
        # Then converting into a dict will automatically clean up lesser
        # important values.
        files = [
          Results.File(None, f, False, None, m)
          for f, m in dict(files).iteritems()
        ]
        return Results.Process(
            self.pid,
            files,
            fix_path(self.executable),
            self.command,
            fix_path(self.initial_cwd),
            [c.to_results_process() for c in self.children])

      def add_file(self, filepath, mode):
        """Adds a file if it passes the blacklist."""
        if self._blacklist(render(filepath)):
          return
        logging.debug('add_file(%d, %s, %s)', self.pid, filepath, mode)
        # Note that filepath and not render(filepath) is added. It is because
        # filepath could be something else than a string, like a RelativePath
        # instance for dtrace logs.
        modes = Results.File.ACCEPTABLE_MODES
        old_mode = self.files.setdefault(filepath, mode)
        if old_mode != mode and modes.index(old_mode) < modes.index(mode):
          # Take the highest value.
          self.files[filepath] = mode

    def __init__(self, blacklist):
      self.blacklist = blacklist
      # Initial process.
      self.root_process = None
      # dict to accelerate process lookup, to not have to lookup the whole graph
      # each time.
      self._process_lookup = {}

  class Tracer(object):
    """During it's lifetime, the tracing subsystem is enabled."""
    def __init__(self, logname):
      self._logname = logname
      self._lock = threading.RLock()
      self._traces = []
      self._initialized = True
      self._scripts_to_cleanup = []

    def trace(self, cmd, cwd, tracename, output):
      """Runs the OS-specific trace program on an executable.

      Arguments:
      - cmd: The command (a list) to run.
      - cwd: Current directory to start the child process in.
      - tracename: Name of the trace in the logname file.
      - output: If False, redirects output to PIPEs.

      Returns a tuple (resultcode, output) and updates the internal trace
      entries.
      """
      # The implementation adds an item to self._traces.
      raise NotImplementedError(self.__class__.__name__)

    def close(self, _timeout=None):
      """Saves the meta-data in the logname file.

      For kernel-based tracing, stops the tracing subsystem.

      Must not be used manually when using 'with' construct.
      """
      with self._lock:
        if not self._initialized:
          raise TracingFailure(
              'Called %s.close() on an unitialized object' %
                  self.__class__.__name__,
              None, None, None)
        try:
          while self._scripts_to_cleanup:
            try:
              fs.remove(self._scripts_to_cleanup.pop())
            except OSError as e:
              logging.error('Failed to delete a temporary script: %s', e)
          tools.write_json(self._logname, self._gen_logdata(), False)
        finally:
          self._initialized = False

    def post_process_log(self):
      """Post-processes the log so it becomes faster to load afterward.

      Must not be used manually when using 'with' construct.
      """
      assert not self._initialized, 'Must stop tracing first.'

    def _gen_logdata(self):
      """Returns the data to be saved in the trace file."""
      return  {
        'traces': self._traces,
      }

    def __enter__(self):
      """Enables 'with' statement."""
      return self

    def __exit__(self, exc_type, exc_value, traceback):
      """Enables 'with' statement."""
      self.close()
      # If an exception was thrown, do not process logs.
      if not exc_type:
        self.post_process_log()

  def get_tracer(self, logname):
    """Returns an ApiBase.Tracer instance.

    Initializes the tracing subsystem, which is a requirement for kernel-based
    tracers. Only one tracer instance should be live at a time!

    logname is the filepath to the json file that will contain the meta-data
    about the logs.
    """
    return self.Tracer(logname)

  @staticmethod
  def clean_trace(logname):
    """Deletes an old log."""
    raise NotImplementedError()

  @classmethod
  def parse_log(cls, logname, blacklist, trace_name):
    """Processes trace logs and returns the files opened and the files that do
    not exist.

    It does not track directories.

    Arguments:
      - logname: must be an absolute path.
      - blacklist: must be a lambda.
      - trace_name: optional trace to read, defaults to reading all traces.

    Most of the time, files that do not exist are temporary test files that
    should be put in /tmp instead. See http://crbug.com/116251.

    Returns a list of dict with keys:
      - results: A Results instance.
      - trace: The corresponding tracename parameter provided to
               get_tracer().trace().
      - output: Output gathered during execution, if get_tracer().trace(...,
                output=False) was used.
    """
    raise NotImplementedError(cls.__class__.__name__)


class Strace(ApiBase):
  """strace implies linux."""
  @staticmethod
  def load_filename(filename):
    """Parses a filename in a log."""
    # TODO(maruel): Be compatible with strace -x.
    assert isinstance(filename, str)
    out = ''
    i = 0
    while i < len(filename):
      c = filename[i]
      if c == '\\':
        out += chr(int(filename[i+1:i+4], 8))
        i += 4
      else:
        out += c
        i += 1
    # TODO(maruel): That's not necessarily true that the current code page is
    # utf-8.
    return out.decode('utf-8')

  class Context(ApiBase.Context):
    """Processes a strace log line and keeps the list of existent and non
    existent files accessed.

    Ignores directories.

    Uses late-binding to processes the cwd of each process. The problem is that
    strace generates one log file per process it traced but doesn't give any
    information about which process was started when and by who. So process the
    logs out of order and use late binding with RelativePath to be able to
    deduce the initial directory of each process once all the logs are parsed.

    TODO(maruel): Use the script even in the non-sudo case, so log parsing can
    be done in two phase: first find the root process, then process the child
    processes in order. With that, it should be possible to not use RelativePath
    anymore. This would significantly simplify the code!
    """
    class Process(ApiBase.Context.Process):
      """Represents the state of a process.

      Contains all the information retrieved from the pid-specific log.
      """
      # Function names are using ([a-z_0-9]+)
      # This is the most common format. function(args) = result
      RE_HEADER = re.compile(r'^([a-z_0-9]+)\((.*?)\)\s+= (.+)$')
      # An interrupted function call, only grab the minimal header.
      RE_UNFINISHED = re.compile(r'^([^\(]+)(.*) \<unfinished \.\.\.\>$')
      # A resumed function call.
      RE_RESUMED = re.compile(r'^<\.\.\. ([^ ]+) resumed> (.+)$')
      # A process received a signal.
      RE_SIGNAL = re.compile(r'^--- SIG[A-Z]+ .+ ---')
      # A process didn't handle a signal. Ignore any junk appearing before,
      # because the process was forcibly killed so it won't open any new file.
      RE_KILLED = re.compile(
          r'^.*\+\+\+ killed by ([A-Z]+)( \(core dumped\))? \+\+\+$')
      # The process has exited.
      RE_PROCESS_EXITED = re.compile(r'^\+\+\+ exited with (\d+) \+\+\+')
      # A call was canceled. Ignore any prefix.
      RE_UNAVAILABLE = re.compile(r'^.*\)\s*= \? <unavailable>$')
      # Happens when strace fails to even get the function name.
      UNNAMED_FUNCTION = '????'

      # Corner-case in python, a class member function decorator must not be
      # @staticmethod.
      def parse_args(regexp, expect_zero):  # pylint: disable=E0213
        """Automatically convert the str 'args' into a list of processed
        arguments.

        Arguments:
        - regexp is used to parse args.
        - expect_zero: one of True, False or None.
          - True: will check for result.startswith('0') first and will ignore
            the trace line completely otherwise. This is important because for
            many functions, the regexp will not process if the call failed.
          - False: will check for not result.startswith(('?', '-1')) for the
            same reason than with True.
          - None: ignore result.
        """
        def meta_hook(function):
          assert function.__name__.startswith('handle_')
          def hook(self, args, result):
            if expect_zero is True and not result.startswith('0'):
              return
            if expect_zero is False and result.startswith(('?', '-1')):
              return
            match = re.match(regexp, args)
            if not match:
              raise TracingFailure(
                  'Failed to parse %s(%s) = %s' %
                  (function.__name__[len('handle_'):], args, result),
                  None, None, None)
            return function(self, match.groups(), result)
          return hook
        return meta_hook

      class RelativePath(object):
        """A late-bound relative path."""
        def __init__(self, parent, value):
          assert_is_renderable(parent)
          self.parent = parent
          assert (
              value is None or
              (isinstance(value, unicode) and not os.path.isabs(value)))
          self.value = value
          if self.value:
            # TODO(maruel): On POSIX, '\\' is a valid character so remove this
            # assert.
            assert '\\' not in self.value, value
            assert '\\' not in self.value, (repr(value), repr(self.value))

        def render(self):
          """Returns the current directory this instance is representing.

          This function is used to return the late-bound value.
          """
          assert self.parent is not None
          parent = render(self.parent)
          if self.value:
            return os.path.normpath(os.path.join(parent, self.value))
          return parent

      def __init__(self, root, pid):
        """Keeps enough information to be able to guess the original process
        root.

        strace doesn't store which process was the initial process. So more
        information needs to be kept so the graph can be reconstructed from the
        flat map.
        """
        logging.info('%s(%d)' % (self.__class__.__name__, pid))
        super(Strace.Context.Process, self).__init__(root.blacklist, pid, None)
        assert isinstance(root, ApiBase.Context)
        self._root = weakref.ref(root)
        # The dict key is the function name of the pending call, like 'open'
        # or 'execve'.
        self._pending_calls = {}
        self._line_number = 0
        # Current directory when the process started.
        if isinstance(self._root(), unicode):
          self.initial_cwd = self._root()
        else:
          self.initial_cwd = self.RelativePath(self._root(), None)
        self.parentid = None
        self._done = False

      def get_cwd(self):
        """Returns the best known value of cwd."""
        return self.cwd or self.initial_cwd

      def render(self):
        """Returns the string value of the RelativePath() object.

        Used by RelativePath. Returns the initial directory and not the
        current one since the current directory 'cwd' validity is time-limited.

        The validity is only guaranteed once all the logs are processed.
        """
        return self.initial_cwd.render()

      def on_line(self, line):
        assert isinstance(line, str)
        self._line_number += 1
        try:
          if self._done:
            raise TracingFailure(
                'Found a trace for a terminated process or corrupted log',
                None, None, None)

          if self.RE_SIGNAL.match(line):
            # Ignore signals.
            return

          match = self.RE_KILLED.match(line)
          if match:
            # Converts a '+++ killed by Foo +++' trace into an exit_group().
            self.handle_exit_group(match.group(1), None)
            return

          match = self.RE_PROCESS_EXITED.match(line)
          if match:
            # Converts a '+++ exited with 1 +++' trace into an exit_group()
            self.handle_exit_group(match.group(1), None)
            return

          match = self.RE_UNFINISHED.match(line)
          if match:
            if match.group(1) in self._pending_calls:
              raise TracingFailure(
                  'Found two unfinished calls for the same function',
                  None, None, None,
                  self._pending_calls)
            self._pending_calls[match.group(1)] = (
                match.group(1) + match.group(2))
            return

          match = self.RE_UNAVAILABLE.match(line)
          if match:
            # This usually means a process was killed and a pending call was
            # canceled.
            # TODO(maruel): Look up the last exit_group() trace just above and
            # make sure any self._pending_calls[anything] is properly flushed.
            return

          match = self.RE_RESUMED.match(line)
          if match:
            if match.group(1) not in self._pending_calls:
              raise TracingFailure(
                  'Found a resumed call that was not logged as unfinished',
                  None, None, None,
                  self._pending_calls)
            pending = self._pending_calls.pop(match.group(1))
            # Reconstruct the line.
            line = pending + match.group(2)

          match = self.RE_HEADER.match(line)
          if not match:
            # The line is corrupted. It happens occasionally when a process is
            # killed forcibly with activity going on. Assume the process died.
            # No other line can be processed afterward.
            logging.debug('%d is done: %s', self.pid, line)
            self._done = True
            return

          if match.group(1) == self.UNNAMED_FUNCTION:
            return

          # It's a valid line, handle it.
          handler = getattr(self, 'handle_%s' % match.group(1), None)
          if not handler:
            self._handle_unknown(match.group(1), match.group(2), match.group(3))
          return handler(match.group(2), match.group(3))
        except TracingFailure, e:
          # Hack in the values since the handler could be a static function.
          e.pid = self.pid
          e.line = line
          e.line_number = self._line_number
          # Re-raise the modified exception.
          raise
        except (KeyError, NotImplementedError, ValueError), e:
          raise TracingFailure(
              'Trace generated a %s exception: %s' % (
                  e.__class__.__name__, str(e)),
              self.pid,
              self._line_number,
              line,
              e)

      @parse_args(r'^\"(.+?)\", [FKORWX_|]+$', True)
      def handle_access(self, args, _result):
        self._handle_file(args[0], Results.File.TOUCHED)

      @parse_args(r'^\"(.+?)\"$', True)
      def handle_chdir(self, args, _result):
        """Updates cwd."""
        self.cwd = self._mangle(args[0])
        logging.debug('handle_chdir(%d, %s)' % (self.pid, self.cwd))

      @parse_args(r'^\"(.+?)\", (\d+), (\d+)$', False)
      def handle_chown(self, args, _result):
        # TODO(maruel): Look at result?
        self._handle_file(args[0], Results.File.WRITE)

      def handle_clone(self, _args, result):
        self._handling_forking('clone', result)

      def handle_close(self, _args, _result):
        pass

      @parse_args(r'^\"(.+?)\", (\d+)$', False)
      def handle_chmod(self, args, _result):
        self._handle_file(args[0], Results.File.WRITE)

      @parse_args(r'^\"(.+?)\", (\d+)$', False)
      def handle_creat(self, args, _result):
        self._handle_file(args[0], Results.File.WRITE)

      @parse_args(r'^\"(.+?)\", \[(.+)\], \[\/\* \d+ vars? \*\/\]$', True)
      def handle_execve(self, args, _result):
        # Even if in practice execve() doesn't returns when it succeeds, strace
        # still prints '0' as the result.
        filepath = args[0]
        self._handle_file(filepath, Results.File.READ)
        self.executable = self._mangle(filepath)
        try:
          self.command = strace_process_quoted_arguments(args[1])
        except ValueError as e:
          raise TracingFailure(
              'Failed to process command line argument:\n%s' % e.args[0],
              None, None, None)

      def handle_exit_group(self, _args, _result):
        """Removes cwd."""
        self.cwd = None

      @parse_args(r'^(\d+|AT_FDCWD), \"(.*?)\", ([A-Z\_\|]+)(|, \d+)$', True)
      def handle_faccessat(self, args, _results):
        if args[0] == 'AT_FDCWD':
          self._handle_file(args[1], Results.File.TOUCHED)
        else:
          raise Exception('Relative faccess not implemented.')

      def handle_fallocate(self, _args, result):
        pass

      def handle_fork(self, args, result):
        self._handle_unknown('fork', args, result)

      def handle_futex(self, _args, _result):
        pass

      @parse_args(r'^(\".+?\"|0x[a-f0-9]+), (\d+)$', False)
      def handle_getcwd(self, args, _result):
        # TODO(maruel): Resolve file handle.
        if not args[0].startswith('0x'):
          filepath = args[0][1:-1]
          if os.path.isabs(filepath):
            logging.debug('handle_chdir(%d, %s)' % (self.pid, self.cwd))
            if not isinstance(self.cwd, unicode):
              # Take the occasion to reset the path.
              self.cwd = self._mangle(filepath)
            else:
              # It should always match.
              assert self.cwd == Strace.load_filename(filepath), (
                  self.cwd, filepath)

      @parse_args(r'^\"(.+?)\", \"(.+?)\"$', True)
      def handle_link(self, args, _result):
        self._handle_file(args[0], Results.File.READ)
        self._handle_file(args[1], Results.File.WRITE)

      @parse_args(r'^(\".+?\"|0x[a-f0-9]+), \{(?:.+?, )?\.\.\.\}$', True)
      def handle_lstat(self, args, _result):
        # TODO(maruel): Resolve file handle.
        if not args[0].startswith('0x'):
          self._handle_file(args[0][1:-1], Results.File.TOUCHED)

      def handle_mkdir(self, _args, _result):
        # We track content, not directories.
        pass

      @parse_args(r'^(\".*?\"|0x[a-f0-9]+), ([A-Z\_\|]+)(|, \d+)$', False)
      def handle_open(self, args, _result):
        if 'O_DIRECTORY' in args[1]:
          return
        # TODO(maruel): Resolve file handle.
        if not args[0].startswith('0x'):
          t = Results.File.READ if 'O_RDONLY' in args[1] else Results.File.WRITE
          self._handle_file(args[0][1:-1], t)

      @parse_args(
          r'^(\d+|AT_FDCWD), (\".*?\"|0x[a-f0-9]+), ([A-Z\_\|]+)(|, \d+)$',
          False)
      def handle_openat(self, args, _result):
        if 'O_DIRECTORY' in args[2]:
          return
        if args[0] == 'AT_FDCWD':
          # TODO(maruel): Resolve file handle.
          if not args[1].startswith('0x'):
            t = (
                Results.File.READ if 'O_RDONLY' in args[2]
                else Results.File.WRITE)
          self._handle_file(args[1][1:-1], t)
        else:
          # TODO(maruel): Implement relative open if necessary instead of the
          # AT_FDCWD flag, let's hope not since this means tracking all active
          # directory handles.
          raise NotImplementedError('Relative open via openat not implemented.')

      @parse_args(
          r'^(\".+?\"|0x[a-f0-9]+), (?:\".+?\"(?:\.\.\.)?|0x[a-f0-9]+), '
            '\d+$',
          False)
      def handle_readlink(self, args, _result):
        # TODO(maruel): Resolve file handle.
        if not args[0].startswith('0x'):
          self._handle_file(args[0][1:-1], Results.File.READ)

      @parse_args(r'^\"(.+?)\", \"(.+?)\"$', True)
      def handle_rename(self, args, _result):
        self._handle_file(args[0], Results.File.READ)
        self._handle_file(args[1], Results.File.WRITE)

      def handle_rmdir(self, _args, _result):
        # We track content, not directories.
        pass

      def handle_setxattr(self, _args, _result):
        # TODO(maruel): self._handle_file(args[0], Results.File.WRITE)
        pass

      @parse_args(r'^(\".+?\"|0x[a-f0-9]+), \{.+?\}$', True)
      def handle_stat(self, args, _result):
        # TODO(maruel): Resolve file handle.
        if not args[0].startswith('0x'):
          self._handle_file(args[0][1:-1], Results.File.TOUCHED)

      @parse_args(r'^\"(.+?)\", \"(.+?)\"$', True)
      def handle_symlink(self, args, _result):
        self._handle_file(args[0], Results.File.TOUCHED)
        self._handle_file(args[1], Results.File.WRITE)

      @parse_args(r'^\"(.+?)\", \d+', True)
      def handle_truncate(self, args, _result):
        self._handle_file(args[0], Results.File.WRITE)

      def handle_unlink(self, _args, _result):
        # In theory, the file had to be created anyway.
        pass

      def handle_unlinkat(self, _args, _result):
        # In theory, the file had to be created anyway.
        pass

      def handle_statfs(self, _args, _result):
        pass

      def handle_utimensat(self, _args, _result):
        pass

      def handle_vfork(self, _args, result):
        self._handling_forking('vfork', result)

      @staticmethod
      def _handle_unknown(function, args, result):
        raise TracingFailure(
            'Unexpected/unimplemented trace %s(%s)= %s' %
            (function, args, result),
            None, None, None)

      def _handling_forking(self, name, result):
        """Transfers cwd."""
        if result.startswith(('?', '-1')):
          # The call failed.
          return
        # Update the other process right away.
        childpid = int(result)
        child = self._root().get_or_set_proc(childpid)
        if child.parentid is not None or childpid in self.children:
          raise TracingFailure(
              'Found internal inconsitency in process lifetime detection '
              'during a %s() call' % name,
              None, None, None)

        # Copy the cwd object.
        child.initial_cwd = self.get_cwd()
        child.parentid = self.pid
        # It is necessary because the logs are processed out of order.
        self.children.append(child)

      def _handle_file(self, filepath, mode):
        filepath = self._mangle(filepath)
        self.add_file(filepath, mode)

      def _mangle(self, filepath):
        """Decodes a filepath found in the log and convert it to a late-bound
        path if necessary.

        |filepath| is an strace 'encoded' string and the returned value is
        either an unicode string if the path was absolute or a late bound path
        otherwise.
        """
        filepath = Strace.load_filename(filepath)
        if os.path.isabs(filepath):
          return filepath
        else:
          if isinstance(self.get_cwd(), unicode):
            return os.path.normpath(os.path.join(self.get_cwd(), filepath))
          return self.RelativePath(self.get_cwd(), filepath)

    def __init__(self, blacklist, root_pid, initial_cwd):
      """|root_pid| may be None when the root process is not known.

      In that case, a search is done after reading all the logs to figure out
      the root process.
      """
      super(Strace.Context, self).__init__(blacklist)
      assert_is_renderable(initial_cwd)
      self.root_pid = root_pid
      self.initial_cwd = initial_cwd

    def render(self):
      """Returns the string value of the initial cwd of the root process.

      Used by RelativePath.
      """
      return self.initial_cwd

    def on_line(self, pid, line):
      """Transfers control into the Process.on_line() function."""
      self.get_or_set_proc(pid).on_line(line.strip())

    def to_results(self):
      """If necessary, finds back the root process and verify consistency."""
      if not self.root_pid:
        # The non-sudo case. The traced process was started by strace itself,
        # so the pid of the traced process is not known.
        root = [p for p in self._process_lookup.itervalues() if not p.parentid]
        if len(root) == 1:
          self.root_process = root[0]
          # Save it for later.
          self.root_pid = self.root_process.pid
      else:
        # The sudo case. The traced process was started manually so its pid is
        # known.
        self.root_process = self._process_lookup.get(self.root_pid)
      if not self.root_process:
        raise TracingFailure(
            'Found internal inconsitency in process lifetime detection '
            'while finding the root process',
            None,
            None,
            None,
            self.root_pid,
            sorted(self._process_lookup))
      process = self.root_process.to_results_process()
      if sorted(self._process_lookup) != sorted(p.pid for p in process.all):
        raise TracingFailure(
            'Found internal inconsitency in process lifetime detection '
            'while looking for len(tree) == len(list)',
            None,
            None,
            None,
            sorted(self._process_lookup),
            sorted(p.pid for p in process.all))
      return Results(process)

    def get_or_set_proc(self, pid):
      """Returns the Context.Process instance for this pid or creates a new one.
      """
      if not pid or not isinstance(pid, int):
        raise TracingFailure(
            'Unpexpected value for pid: %r' % pid,
            pid,
            None,
            None,
            pid)
      if pid not in self._process_lookup:
        self._process_lookup[pid] = self.Process(self, pid)
      return self._process_lookup[pid]

    @classmethod
    def traces(cls):
      """Returns the list of all handled traces to pass this as an argument to
      strace.
      """
      prefix = 'handle_'
      return [i[len(prefix):] for i in dir(cls.Process) if i.startswith(prefix)]

  class Tracer(ApiBase.Tracer):
    MAX_LEN = 256

    def __init__(self, logname, use_sudo):
      super(Strace.Tracer, self).__init__(logname)
      self.use_sudo = use_sudo
      if use_sudo:
        # TODO(maruel): Use the jump script systematically to make it easy to
        # figure out the root process, so RelativePath is not necessary anymore.
        self._child_script = create_exec_thunk()
        self._scripts_to_cleanup.append(self._child_script)

    def trace(self, cmd, cwd, tracename, output):
      """Runs strace on an executable.

      When use_sudo=True, it is a 3-phases process: start the thunk, start
      sudo strace with the pid of the thunk and then have the thunk os.execve()
      the process to trace.
      """
      logging.info('trace(%s, %s, %s, %s)' % (cmd, cwd, tracename, output))
      assert os.path.isabs(cmd[0]), cmd[0]
      assert os.path.isabs(cwd), cwd
      assert os.path.normpath(cwd) == cwd, cwd
      with self._lock:
        if not self._initialized:
          raise TracingFailure(
              'Called Tracer.trace() on an unitialized object',
              None, None, None, tracename)
        assert tracename not in (i['trace'] for i in self._traces)
      stdout = stderr = None
      if output:
        stdout = subprocess.PIPE
        stderr = subprocess.STDOUT

      # Ensure all file related APIs are hooked.
      traces = ','.join(Strace.Context.traces() + ['file'])
      flags = [
        # Each child process has its own trace file. It is necessary because
        # strace may generate corrupted log file if multiple processes are
        # heavily doing syscalls simultaneously.
        '-ff',
        # Reduce whitespace usage.
        '-a1',
        # hex encode non-ascii strings.
        # TODO(maruel): '-x',
        # TODO(maruel): '-ttt',
        # Signals are unnecessary noise here. Note the parser can cope with them
        # but reduce the unnecessary output.
        '-esignal=none',
        # Print as much data as wanted.
        '-s', '%d' % self.MAX_LEN,
        '-e', 'trace=%s' % traces,
        '-o', self._logname + '.' + tracename,
      ]

      if self.use_sudo:
        pipe_r, pipe_w = os.pipe()
        # Start the child process paused.
        target_cmd = [sys.executable, self._child_script, str(pipe_r)] + cmd
        logging.debug(' '.join(target_cmd))
        child_proc = subprocess.Popen(
            target_cmd,
            stdin=subprocess.PIPE,
            stdout=stdout,
            stderr=stderr,
            cwd=cwd)

        # TODO(maruel): both processes must use the same UID for it to work
        # without sudo. Look why -p is failing at the moment without sudo.
        trace_cmd = [
          'sudo',
          'strace',
          '-p', str(child_proc.pid),
        ] + flags
        logging.debug(' '.join(trace_cmd))
        strace_proc = subprocess.Popen(
            trace_cmd,
            cwd=cwd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

        line = strace_proc.stderr.readline()
        if not re.match(r'^Process \d+ attached \- interrupt to quit$', line):
          # TODO(maruel): Raise an exception.
          assert False, line

        # Now fire the child process.
        os.write(pipe_w, 'x')

        out = child_proc.communicate()[0]
        strace_out = strace_proc.communicate()[0]

        # TODO(maruel): if strace_proc.returncode: Add an exception.
        saved_out = strace_out if strace_proc.returncode else out
        root_pid = child_proc.pid
      else:
        # Non-sudo case.
        trace_cmd = [
          'strace',
        ] + flags + cmd
        logging.debug(' '.join(trace_cmd))
        child_proc = subprocess.Popen(
            trace_cmd,
            cwd=cwd,
            stdin=subprocess.PIPE,
            stdout=stdout,
            stderr=stderr)
        out = child_proc.communicate()[0]
        # TODO(maruel): Walk the logs and figure out the root process would
        # simplify parsing the logs a *lot*.
        saved_out = out
        # The trace reader will have to figure out.
        root_pid = None

      with self._lock:
        assert tracename not in (i['trace'] for i in self._traces)
        self._traces.append(
          {
            'cmd': cmd,
            'cwd': cwd,
            'output': saved_out,
            'pid': root_pid,
            'trace': tracename,
          })
      return child_proc.returncode, out

  def __init__(self, use_sudo=None):
    super(Strace, self).__init__()
    self.use_sudo = use_sudo

  def get_tracer(self, logname):
    return self.Tracer(logname, self.use_sudo)

  @staticmethod
  def clean_trace(logname):
    if fs.isfile(logname):
      fs.remove(logname)
    # Also delete any pid specific file from previous traces.
    for i in glob.iglob(logname + '.*'):
      if i.rsplit('.', 1)[1].isdigit():
        fs.remove(i)

  @classmethod
  def parse_log(cls, logname, blacklist, trace_name):
    logging.info('parse_log(%s, ..., %s)', logname, trace_name)
    assert os.path.isabs(logname)
    data = tools.read_json(logname)
    out = []
    for item in data['traces']:
      if trace_name and item['trace'] != trace_name:
        continue
      result = {
        'output': item['output'],
        'trace': item['trace'],
      }
      try:
        context = cls.Context(blacklist, item['pid'], item['cwd'])
        for pidfile in glob.iglob('%s.%s.*' % (logname, item['trace'])):
          logging.debug('Reading %s', pidfile)
          pid = pidfile.rsplit('.', 1)[1]
          if pid.isdigit():
            pid = int(pid)
            found_line = False
            for line in open(pidfile, 'rb'):
              context.on_line(pid, line)
              found_line = True
            if not found_line:
              # Ensures that a completely empty trace still creates the
              # corresponding Process instance by logging a dummy line.
              context.on_line(pid, '')
          else:
            logging.warning('Found unexpected file %s', pidfile)
        result['results'] = context.to_results()
      except TracingFailure:
        result['exception'] = sys.exc_info()
      out.append(result)
    return out


class Dtrace(ApiBase):
  """Uses DTrace framework through dtrace. Requires root access.

  Implies Mac OSX.

  dtruss can't be used because it has compatibility issues with python.

  Also, the pid->cwd handling needs to be done manually since OSX has no way to
  get the absolute path of the 'cwd' dtrace variable from the probe.

  Also, OSX doesn't populate curpsinfo->pr_psargs properly, see
  https://discussions.apple.com/thread/1980539. So resort to handling execve()
  manually.

  errno is not printed in the log since this implementation currently only cares
  about files that were successfully opened.
  """
  class Context(ApiBase.Context):
    # Format: index pid function(args)
    RE_HEADER = re.compile(r'^\d+ (\d+) ([a-zA-Z_\-]+)\((.*?)\)$')

    # Arguments parsing.
    RE_DTRACE_BEGIN = re.compile(r'^\"(.+?)\"$')
    RE_CHDIR = re.compile(r'^\"(.+?)\"$')
    RE_EXECVE = re.compile(r'^\"(.+?)\", \[(\d+), (.+)\]$')
    RE_OPEN = re.compile(r'^\"(.+?)\", (0x[0-9a-z]+), (0x[0-9a-z]+)$')
    RE_PROC_START = re.compile(r'^(\d+), \"(.+?)\", (\d+)$')
    RE_RENAME = re.compile(r'^\"(.+?)\", \"(.+?)\"$')

    # O_DIRECTORY is not defined on Windows and dtrace doesn't exist on Windows.
    O_DIRECTORY = os.O_DIRECTORY if hasattr(os, 'O_DIRECTORY') else None
    O_RDWR = os.O_RDWR
    O_WRONLY = os.O_WRONLY

    class Process(ApiBase.Context.Process):
      def __init__(self, *args):
        super(Dtrace.Context.Process, self).__init__(*args)
        self.cwd = self.initial_cwd

    def __init__(self, blacklist, thunk_pid, initial_cwd):
      logging.info(
          '%s(%d, %s)' % (self.__class__.__name__, thunk_pid, initial_cwd))
      super(Dtrace.Context, self).__init__(blacklist)
      assert isinstance(initial_cwd, unicode), initial_cwd
      # Process ID of the temporary script created by create_subprocess_thunk().
      self._thunk_pid = thunk_pid
      self._initial_cwd = initial_cwd
      self._line_number = 0

    def on_line(self, line):
      assert isinstance(line, unicode), line
      self._line_number += 1
      match = self.RE_HEADER.match(line)
      if not match:
        raise TracingFailure(
            'Found malformed line: %s' % line,
            None,
            self._line_number,
            line)
      fn = getattr(
          self,
          'handle_%s' % match.group(2).replace('-', '_'),
          self._handle_ignored)
      # It is guaranteed to succeed because of the regexp. Or at least I thought
      # it would.
      pid = int(match.group(1))
      try:
        return fn(pid, match.group(3))
      except TracingFailure, e:
        # Hack in the values since the handler could be a static function.
        e.pid = pid
        e.line = line
        e.line_number = self._line_number
        # Re-raise the modified exception.
        raise
      except (KeyError, NotImplementedError, ValueError), e:
        raise TracingFailure(
            'Trace generated a %s exception: %s' % (
                e.__class__.__name__, str(e)),
            pid,
            self._line_number,
            line,
            e)

    def to_results(self):
      process = self.root_process.to_results_process()
      # Internal concistency check.
      if sorted(self._process_lookup) != sorted(p.pid for p in process.all):
        raise TracingFailure(
            'Found internal inconsitency in process lifetime detection '
            'while looking for len(tree) == len(list)',
            None,
            None,
            None,
            sorted(self._process_lookup),
            sorted(p.pid for p in process.all))
      return Results(process)

    def handle_dtrace_BEGIN(self, _pid, args):
      if not self.RE_DTRACE_BEGIN.match(args):
        raise TracingFailure(
            'Found internal inconsitency in dtrace_BEGIN log line',
            None, None, None)

    def handle_proc_start(self, pid, args):
      """Transfers cwd.

      The dtrace script already takes care of only tracing the processes that
      are child of the traced processes so there is no need to verify the
      process hierarchy.
      """
      if pid in self._process_lookup:
        raise TracingFailure(
            'Found internal inconsitency in proc_start: %d started two times' %
                pid,
            None, None, None)
      match = self.RE_PROC_START.match(args)
      if not match:
        raise TracingFailure(
            'Failed to parse arguments: %s' % args,
            None, None, None)
      ppid = int(match.group(1))
      if ppid == self._thunk_pid and not self.root_process:
        proc = self.root_process = self.Process(
            self.blacklist, pid, self._initial_cwd)
      elif ppid in self._process_lookup:
        proc = self.Process(self.blacklist, pid, self._process_lookup[ppid].cwd)
        self._process_lookup[ppid].children.append(proc)
      else:
        # Another process tree, ignore.
        return
      self._process_lookup[pid] = proc
      logging.debug(
          'New child: %s -> %d  cwd:%s' %
          (ppid, pid, render(proc.initial_cwd)))

    def handle_proc_exit(self, pid, _args):
      """Removes cwd."""
      if pid in self._process_lookup:
        # self._thunk_pid is not traced itself and other traces run neither.
        self._process_lookup[pid].cwd = None

    def handle_execve(self, pid, args):
      """Sets the process' executable.

      TODO(maruel): Read command line arguments.  See
      https://discussions.apple.com/thread/1980539 for an example.
      https://gist.github.com/1242279

      Will have to put the answer at http://stackoverflow.com/questions/7556249.
      :)
      """
      if not pid in self._process_lookup:
        # Another process tree, ignore.
        return
      match = self.RE_EXECVE.match(args)
      if not match:
        raise TracingFailure(
            'Failed to parse arguments: %r' % args,
            None, None, None)
      proc = self._process_lookup[pid]
      proc.executable = match.group(1)
      self._handle_file(pid, proc.executable, Results.File.READ)
      proc.command = self.process_escaped_arguments(match.group(3))
      if int(match.group(2)) != len(proc.command):
        raise TracingFailure(
            'Failed to parse execve() arguments: %s' % args,
            None, None, None)

    def handle_chdir(self, pid, args):
      """Updates cwd."""
      if pid not in self._process_lookup:
        # Another process tree, ignore.
        return
      cwd = self.RE_CHDIR.match(args).group(1)
      if not cwd.startswith('/'):
        cwd2 = os.path.join(self._process_lookup[pid].cwd, cwd)
        logging.debug('handle_chdir(%d, %s) -> %s' % (pid, cwd, cwd2))
      else:
        logging.debug('handle_chdir(%d, %s)' % (pid, cwd))
        cwd2 = cwd
      self._process_lookup[pid].cwd = cwd2

    def handle_open_nocancel(self, pid, args):
      """Redirects to handle_open()."""
      return self.handle_open(pid, args)

    def handle_open(self, pid, args):
      if pid not in self._process_lookup:
        # Another process tree, ignore.
        return
      match = self.RE_OPEN.match(args)
      if not match:
        raise TracingFailure(
            'Failed to parse arguments: %s' % args,
            None, None, None)
      flag = int(match.group(2), 16)
      if self.O_DIRECTORY & flag == self.O_DIRECTORY:
        # Ignore directories.
        return
      self._handle_file(
          pid,
          match.group(1),
          Results.File.READ if not ((self.O_RDWR | self.O_WRONLY) & flag)
            else Results.File.WRITE)

    def handle_rename(self, pid, args):
      if pid not in self._process_lookup:
        # Another process tree, ignore.
        return
      match = self.RE_RENAME.match(args)
      if not match:
        raise TracingFailure(
            'Failed to parse arguments: %s' % args,
            None, None, None)
      self._handle_file(pid, match.group(1), Results.File.READ)
      self._handle_file(pid, match.group(2), Results.File.WRITE)

    def _handle_file(self, pid, filepath, mode):
      if not filepath.startswith('/'):
        filepath = os.path.join(self._process_lookup[pid].cwd, filepath)
      # We can get '..' in the path.
      filepath = os.path.normpath(filepath)
      # Sadly, still need to filter out directories here;
      # saw open_nocancel(".", 0, 0) = 0 lines.
      if fs.isdir(filepath):
        return
      self._process_lookup[pid].add_file(filepath, mode)

    def handle_ftruncate(self, pid, args):
      """Just used as a signal to kill dtrace, ignoring."""
      pass

    @staticmethod
    def _handle_ignored(pid, args):
      """Is called for all the event traces that are not handled."""
      raise NotImplementedError('Please implement me')

    @staticmethod
    def process_escaped_arguments(text):
      """Extracts escaped arguments on a string and return the arguments as a
      list.

      Implemented as an automaton.

      Example:
        With text = '\\001python2.7\\001-c\\001print(\\"hi\\")\\0', the
        function will return ['python2.7', '-c', 'print("hi")]
      """
      if not text.endswith('\\0'):
        raise ValueError('String is not null terminated: %r' % text, text)
      text = text[:-2]

      def unescape(x):
        """Replaces '\\' with '\' and '\?' (where ? is anything) with ?."""
        out = []
        escaped = False
        for i in x:
          if i == '\\' and not escaped:
            escaped = True
            continue
          escaped = False
          out.append(i)
        return ''.join(out)

      return [unescape(i) for i in text.split('\\001')]

  class Tracer(ApiBase.Tracer):
    # pylint: disable=C0301
    #
    # To understand the following code, you'll want to take a look at:
    # http://developers.sun.com/solaris/articles/dtrace_quickref/dtrace_quickref.html
    # https://wikis.oracle.com/display/DTrace/Variables
    # http://docs.oracle.com/cd/E19205-01/820-4221/
    #
    # 0. Dump all the valid probes into a text file. It is important, you
    #    want to redirect into a file and you don't want to constantly 'sudo'.
    # $ sudo dtrace -l > probes.txt
    #
    # 1. Count the number of probes:
    # $ wc -l probes.txt
    # 81823  # On OSX 10.7, including 1 header line.
    #
    # 2. List providers, intentionally skipping all the 'syspolicy10925' and the
    #    likes and skipping the header with NR>1:
    # $ awk 'NR>1 { print $2 }' probes.txt | sort | uniq | grep -v '[[:digit:]]'
    # dtrace
    # fbt
    # io
    # ip
    # lockstat
    # mach_trap
    # proc
    # profile
    # sched
    # syscall
    # tcp
    # vminfo
    #
    # 3. List of valid probes:
    # $ grep syscall probes.txt | less
    #    or use dtrace directly:
    # $ sudo dtrace -l -P syscall | less
    #
    # trackedpid is an associative array where its value can be 0, 1 or 2.
    # 0 is for untracked processes and is the default value for items not
    #   in the associative array.
    # 1 is for tracked processes.
    # 2 is for the script created by create_subprocess_thunk() only. It is not
    #   tracked itself but all its decendants are.
    #
    # The script will kill itself only once waiting_to_die == 1 and
    # current_processes == 0, so that both getlogin() was called and that
    # all traced processes exited.
    #
    # TODO(maruel): Use cacheable predicates. See
    # https://wikis.oracle.com/display/DTrace/Performance+Considerations
    D_CODE = """
      dtrace:::BEGIN {
        waiting_to_die = 0;
        current_processes = 0;
        logindex = 0;
        printf("%d %d %s_%s(\\"%s\\")\\n",
               logindex, PID, probeprov, probename, SCRIPT);
        logindex++;
      }

      proc:::start /trackedpid[ppid]/ {
        trackedpid[pid] = 1;
        current_processes += 1;
        printf("%d %d %s_%s(%d, \\"%s\\", %d)\\n",
               logindex, pid, probeprov, probename,
               ppid,
               execname,
               current_processes);
        logindex++;
      }
      /* Should use SCRIPT but there is no access to this variable at that
       * point. */
      proc:::start /ppid == PID && execname == "Python"/ {
        trackedpid[pid] = 2;
        current_processes += 1;
        printf("%d %d %s_%s(%d, \\"%s\\", %d)\\n",
               logindex, pid, probeprov, probename,
               ppid,
               execname,
               current_processes);
        logindex++;
      }
      proc:::exit /trackedpid[pid] &&
                  current_processes == 1 &&
                  waiting_to_die == 1/ {
        trackedpid[pid] = 0;
        current_processes -= 1;
        printf("%d %d %s_%s(%d)\\n",
               logindex, pid, probeprov, probename,
               current_processes);
        logindex++;
        exit(0);
      }
      proc:::exit /trackedpid[pid]/ {
        trackedpid[pid] = 0;
        current_processes -= 1;
        printf("%d %d %s_%s(%d)\\n",
               logindex, pid, probeprov, probename,
               current_processes);
        logindex++;
      }

      /* Use an arcane function to detect when we need to die */
      syscall::ftruncate:entry /pid == PID && arg0 == FILE_ID/ {
        waiting_to_die = 1;
        printf("%d %d %s()\\n", logindex, pid, probefunc);
        logindex++;
      }
      syscall::ftruncate:entry /
          pid == PID && arg0 == FILE_ID && current_processes == 0/ {
        exit(0);
      }

      syscall::open*:entry /trackedpid[pid] == 1/ {
        self->open_arg0 = arg0;
        self->open_arg1 = arg1;
        self->open_arg2 = arg2;
      }
      syscall::open*:return /trackedpid[pid] == 1 && errno == 0/ {
        this->open_arg0 = copyinstr(self->open_arg0);
        printf("%d %d %s(\\"%s\\", 0x%x, 0x%x)\\n",
               logindex, pid, probefunc,
               this->open_arg0,
               self->open_arg1,
               self->open_arg2);
        logindex++;
        this->open_arg0 = 0;
      }
      syscall::open*:return /trackedpid[pid] == 1/ {
        self->open_arg0 = 0;
        self->open_arg1 = 0;
        self->open_arg2 = 0;
      }

      syscall::rename:entry /trackedpid[pid] == 1/ {
        self->rename_arg0 = arg0;
        self->rename_arg1 = arg1;
      }
      syscall::rename:return /trackedpid[pid] == 1 && errno == 0/ {
        this->rename_arg0 = copyinstr(self->rename_arg0);
        this->rename_arg1 = copyinstr(self->rename_arg1);
        printf("%d %d %s(\\"%s\\", \\"%s\\")\\n",
               logindex, pid, probefunc,
               this->rename_arg0,
               this->rename_arg1);
        logindex++;
        this->rename_arg0 = 0;
        this->rename_arg1 = 0;
      }
      syscall::rename:return /trackedpid[pid] == 1/ {
        self->rename_arg0 = 0;
        self->rename_arg1 = 0;
      }

      /* Track chdir, it's painful because it is only receiving relative path.
       */
      syscall::chdir:entry /trackedpid[pid] == 1/ {
        self->chdir_arg0 = arg0;
      }
      syscall::chdir:return /trackedpid[pid] == 1 && errno == 0/ {
        this->chdir_arg0 = copyinstr(self->chdir_arg0);
        printf("%d %d %s(\\"%s\\")\\n",
               logindex, pid, probefunc,
               this->chdir_arg0);
        logindex++;
        this->chdir_arg0 = 0;
      }
      syscall::chdir:return /trackedpid[pid] == 1/ {
        self->chdir_arg0 = 0;
      }
      """

    # execve-specific code, tends to throw a lot of exceptions.
    D_CODE_EXECVE = """
      /* Finally what we care about! */
      syscall::exec*:entry /trackedpid[pid]/ {
        self->exec_arg0 = copyinstr(arg0);
        /* Incrementally probe for a NULL in the argv parameter of execve() to
         * figure out argc. */
        /* TODO(maruel): Skip the remaining copyin() when a NULL pointer was
         * found. */
        self->exec_argc = 0;
        /* Probe for argc==1 */
        this->exec_argv = (user_addr_t*)copyin(
             arg1, sizeof(user_addr_t) * (self->exec_argc + 1));
        self->exec_argc = this->exec_argv[self->exec_argc] ?
            (self->exec_argc + 1) : self->exec_argc;

        /* Probe for argc==2 */
        this->exec_argv = (user_addr_t*)copyin(
             arg1, sizeof(user_addr_t) * (self->exec_argc + 1));
        self->exec_argc = this->exec_argv[self->exec_argc] ?
            (self->exec_argc + 1) : self->exec_argc;

        /* Probe for argc==3 */
        this->exec_argv = (user_addr_t*)copyin(
             arg1, sizeof(user_addr_t) * (self->exec_argc + 1));
        self->exec_argc = this->exec_argv[self->exec_argc] ?
            (self->exec_argc + 1) : self->exec_argc;

        /* Probe for argc==4 */
        this->exec_argv = (user_addr_t*)copyin(
             arg1, sizeof(user_addr_t) * (self->exec_argc + 1));
        self->exec_argc = this->exec_argv[self->exec_argc] ?
            (self->exec_argc + 1) : self->exec_argc;

        /* Copy the inputs strings since there is no guarantee they'll be
         * present after the call completed. */
        self->exec_argv0 = (self->exec_argc > 0) ?
            copyinstr(this->exec_argv[0]) : "";
        self->exec_argv1 = (self->exec_argc > 1) ?
            copyinstr(this->exec_argv[1]) : "";
        self->exec_argv2 = (self->exec_argc > 2) ?
            copyinstr(this->exec_argv[2]) : "";
        self->exec_argv3 = (self->exec_argc > 3) ?
            copyinstr(this->exec_argv[3]) : "";
        this->exec_argv = 0;
      }
      syscall::exec*:return /trackedpid[pid] && errno == 0/ {
        /* We need to join strings here, as using multiple printf() would
         * cause tearing when multiple threads/processes are traced.
         * Since it is impossible to escape a string and join it to another one,
         * like sprintf("%s%S", previous, more), use hackery.
         * Each of the elements are split with a \\1. \\0 cannot be used because
         * it is simply ignored. This will conflict with any program putting a
         * \\1 in their execve() string but this should be "rare enough" */
        this->args = "";
        /* Process exec_argv[0] */
        this->args = strjoin(
            this->args, (self->exec_argc > 0) ? self->exec_argv0 : "");

        /* Process exec_argv[1] */
        this->args = strjoin(
            this->args, (self->exec_argc > 1) ? "\\1" : "");
        this->args = strjoin(
            this->args, (self->exec_argc > 1) ? self->exec_argv1 : "");

        /* Process exec_argv[2] */
        this->args = strjoin(
            this->args, (self->exec_argc > 2) ? "\\1" : "");
        this->args = strjoin(
            this->args, (self->exec_argc > 2) ? self->exec_argv2 : "");

        /* Process exec_argv[3] */
        this->args = strjoin(
            this->args, (self->exec_argc > 3) ? "\\1" : "");
        this->args = strjoin(
            this->args, (self->exec_argc > 3) ? self->exec_argv3 : "");

        /* Prints self->exec_argc to permits verifying the internal
         * consistency since this code is quite fishy. */
        printf("%d %d %s(\\"%s\\", [%d, %S])\\n",
               logindex, pid, probefunc,
               self->exec_arg0,
               self->exec_argc,
               this->args);
        logindex++;
        this->args = 0;
      }
      syscall::exec*:return /trackedpid[pid]/ {
        self->exec_arg0 = 0;
        self->exec_argc = 0;
        self->exec_argv0 = 0;
        self->exec_argv1 = 0;
        self->exec_argv2 = 0;
        self->exec_argv3 = 0;
      }
      """

    # Code currently not used.
    D_EXTRANEOUS = """
      /* This is a good learning experience, since it traces a lot of things
       * related to the process and child processes.
       * Warning: it generates a gigantic log. For example, tracing
       * "data/trace_inputs/child1.py --child" generates a 2mb log and takes
       * several minutes to execute.
       */
      /*
      mach_trap::: /trackedpid[pid] == 1 || trackedpid[ppid]/ {
        printf("%d %d %s_%s() = %d\\n",
               logindex, pid, probeprov, probefunc, errno);
        logindex++;
      }
      proc::: /trackedpid[pid] == 1 || trackedpid[ppid]/ {
        printf("%d %d %s_%s() = %d\\n",
               logindex, pid, probeprov, probefunc, errno);
        logindex++;
      }
      sched::: /trackedpid[pid] == 1 || trackedpid[ppid]/ {
        printf("%d %d %s_%s() = %d\\n",
               logindex, pid, probeprov, probefunc, errno);
        logindex++;
      }
      syscall::: /trackedpid[pid] == 1 || trackedpid[ppid]/ {
        printf("%d %d %s_%s() = %d\\n",
               logindex, pid, probeprov, probefunc, errno);
        logindex++;
      }
      vminfo::: /trackedpid[pid] == 1 || trackedpid[ppid]/ {
        printf("%d %d %s_%s() = %d\\n",
               logindex, pid, probeprov, probefunc, errno);
        logindex++;
      }
      */
      /* TODO(maruel): *stat* functions and friends
        syscall::access:return,
        syscall::chdir:return,
        syscall::chflags:return,
        syscall::chown:return,
        syscall::chroot:return,
        syscall::getattrlist:return,
        syscall::getxattr:return,
        syscall::lchown:return,
        syscall::lstat64:return,
        syscall::lstat:return,
        syscall::mkdir:return,
        syscall::pathconf:return,
        syscall::readlink:return,
        syscall::removexattr:return,
        syscall::setxattr:return,
        syscall::stat64:return,
        syscall::stat:return,
        syscall::truncate:return,
        syscall::unlink:return,
        syscall::utimes:return,
      */
      """

    def __init__(self, logname, use_sudo):
      """Starts the log collection with dtrace.

      Requires root access or chmod 4555 on dtrace. dtrace is asynchronous so
      this needs to wait for dtrace to be "warmed up".
      """
      super(Dtrace.Tracer, self).__init__(logname)
      # This script is used as a signal to figure out the root process.
      self._signal_script = create_subprocess_thunk()
      self._scripts_to_cleanup.append(self._signal_script)
      # This unique dummy temp file is used to signal the dtrace script that it
      # should stop as soon as all the child processes are done. A bit hackish
      # but works fine enough.
      self._dummy_file_id, self._dummy_file_name = tempfile.mkstemp(
          prefix=u'trace_signal_file')

      dtrace_path = '/usr/sbin/dtrace'
      if not fs.isfile(dtrace_path):
        dtrace_path = 'dtrace'
      elif use_sudo is None and (fs.stat(dtrace_path).st_mode & stat.S_ISUID):
        # No need to sudo. For those following at home, don't do that.
        use_sudo = False

      # Note: do not use the -p flag. It's useless if the initial process quits
      # too fast, resulting in missing traces from the grand-children. The D
      # code manages the dtrace lifetime itself.
      trace_cmd = [
        dtrace_path,
        # Use a larger buffer if getting 'out of scratch space' errors.
        # Ref: https://wikis.oracle.com/display/DTrace/Options+and+Tunables
        '-b', '10m',
        '-x', 'dynvarsize=10m',
        #'-x', 'dtrace_global_maxsize=1m',
        '-x', 'evaltime=exec',
        '-o', '/dev/stderr',
        '-q',
        '-n', self._get_dtrace_code(),
      ]
      if use_sudo is not False:
        trace_cmd.insert(0, 'sudo')

      with fs.open(self._logname + '.log', 'wb') as logfile:
        self._dtrace = subprocess.Popen(
            trace_cmd, stdout=logfile, stderr=subprocess.STDOUT)
      logging.debug('Started dtrace pid: %d' % self._dtrace.pid)

      # Reads until one line is printed, which signifies dtrace is up and ready.
      with fs.open(self._logname + '.log', 'rb') as logfile:
        while 'dtrace_BEGIN' not in logfile.readline():
          if self._dtrace.poll() is not None:
            # Do a busy wait. :/
            break
      logging.debug('dtrace started')

    def _get_dtrace_code(self):
      """Setups the D code to implement child process tracking.

      Injects the cookie in the script so it knows when to stop.

      The script will detect any instance of the script created with
      create_subprocess_thunk() and will start tracing it.
      """
      out = (
          'inline int PID = %d;\n'
          'inline string SCRIPT = "%s";\n'
          'inline int FILE_ID = %d;\n'
          '\n'
          '%s') % (
              os.getpid(),
              self._signal_script,
              self._dummy_file_id,
              self.D_CODE)
      if os.environ.get('TRACE_INPUTS_DTRACE_ENABLE_EXECVE') == '1':
        # Do not enable by default since it tends to spew dtrace: error lines
        # because the execve() parameters are not in valid memory at the time of
        # logging.
        # TODO(maruel): Find a way to make this reliable since it's useful but
        # only works in limited/trivial uses cases for now.
        out += self.D_CODE_EXECVE
      return out

    def trace(self, cmd, cwd, tracename, output):
      """Runs dtrace on an executable.

      This dtruss is broken when it starts the process itself or when tracing
      child processes, this code starts a wrapper process
      generated with create_subprocess_thunk() which starts the executable to
      trace.
      """
      logging.info('trace(%s, %s, %s, %s)' % (cmd, cwd, tracename, output))
      assert os.path.isabs(cmd[0]), cmd[0]
      assert os.path.isabs(cwd), cwd
      assert os.path.normpath(cwd) == cwd, cwd
      with self._lock:
        if not self._initialized:
          raise TracingFailure(
              'Called Tracer.trace() on an unitialized object',
              None, None, None, tracename)
        assert tracename not in (i['trace'] for i in self._traces)

      # Starts the script wrapper to start the child process. This signals the
      # dtrace script that this process is to be traced.
      stdout = stderr = None
      if output:
        stdout = subprocess.PIPE
        stderr = subprocess.STDOUT
      child_cmd = [
        sys.executable,
        self._signal_script,
        tracename,
      ]
      # Call a dummy function so that dtrace knows I'm about to launch a process
      # that needs to be traced.
      # Yummy.
      child = subprocess.Popen(
          child_cmd + tools.fix_python_path(cmd),
          stdin=subprocess.PIPE,
          stdout=stdout,
          stderr=stderr,
          cwd=cwd)
      logging.debug('Started child pid: %d' % child.pid)

      out = child.communicate()[0]
      # This doesn't mean tracing is done, one of the grand-child process may
      # still be alive. It will be tracked with the dtrace script.

      with self._lock:
        assert tracename not in (i['trace'] for i in self._traces)
        self._traces.append(
          {
            'cmd': cmd,
            'cwd': cwd,
            'pid': child.pid,
            'output': out,
            'trace': tracename,
          })
      return child.returncode, out

    def close(self, timeout=None):
      """Terminates dtrace."""
      logging.debug('close(%s)' % timeout)
      try:
        try:
          super(Dtrace.Tracer, self).close(timeout)
          # Signal dtrace that it should stop now.
          # ftruncate doesn't exist on Windows.
          os.ftruncate(self._dummy_file_id, 0)  # pylint: disable=E1101
          if timeout:
            start = time.time()
            # Use polling. :/
            while (self._dtrace.poll() is None and
                   (time.time() - start) < timeout):
              time.sleep(0.1)
            self._dtrace.kill()
          self._dtrace.wait()
        finally:
          # Make sure to kill it in any case.
          if self._dtrace.poll() is None:
            try:
              self._dtrace.kill()
              self._dtrace.wait()
            except OSError:
              pass

        if self._dtrace.returncode != 0:
          # Warn about any dtrace failure but basically ignore it.
          print 'dtrace failure: %s' % self._dtrace.returncode
      finally:
        os.close(self._dummy_file_id)
        fs.remove(self._dummy_file_name)

    def post_process_log(self):
      """Sorts the log back in order when each call occured.

      dtrace doesn't save the buffer in strict order since it keeps one buffer
      per CPU.
      """
      super(Dtrace.Tracer, self).post_process_log()
      logname = self._logname + '.log'
      with fs.open(logname, 'rb') as logfile:
        lines = [l for l in logfile if l.strip()]
      errors = [l for l in lines if l.startswith('dtrace:')]
      if errors:
        raise TracingFailure(
            'Found errors in the trace: %s' % '\n'.join(errors),
            None, None, None, logname)
      try:
        lines = sorted(lines, key=lambda l: int(l.split(' ', 1)[0]))
      except ValueError:
        raise TracingFailure(
            'Found errors in the trace: %s' % '\n'.join(
                l for l in lines if l.split(' ', 1)[0].isdigit()),
            None, None, None, logname)
      with fs.open(logname, 'wb') as logfile:
        logfile.write(''.join(lines))

  def __init__(self, use_sudo=None):
    super(Dtrace, self).__init__()
    self.use_sudo = use_sudo

  def get_tracer(self, logname):
    return self.Tracer(logname, self.use_sudo)

  @staticmethod
  def clean_trace(logname):
    for ext in ('', '.log'):
      if fs.isfile(logname + ext):
        fs.remove(logname + ext)

  @classmethod
  def parse_log(cls, logname, blacklist, trace_name):
    logging.info('parse_log(%s, ..., %s)', logname, trace_name)
    assert os.path.isabs(logname)

    def blacklist_more(filepath):
      # All the HFS metadata is in the form /.vol/...
      return blacklist(filepath) or re.match(r'^\/\.vol\/.+$', filepath)

    data = tools.read_json(logname)
    out = []
    for item in data['traces']:
      if trace_name and item['trace'] != trace_name:
        continue
      result = {
        'output': item['output'],
        'trace': item['trace'],
      }
      try:
        context = cls.Context(blacklist_more, item['pid'], item['cwd'])
        # It's fine to assume the file as UTF-8: OSX enforces the file names to
        # be valid UTF-8 and we control the log output.
        for line in codecs.open(logname + '.log', 'rb', encoding='utf-8'):
          context.on_line(line)
        result['results'] = context.to_results()
      except TracingFailure:
        result['exception'] = sys.exc_info()
      out.append(result)
    return out


class LogmanTrace(ApiBase):
  """Uses the native Windows ETW based tracing functionality to trace a child
  process.

  Caveat: this implementations doesn't track cwd or initial_cwd. It is because
  the Windows Kernel doesn't have a concept of 'current working directory' at
  all. A Win32 process has a map of current directories, one per drive letter
  and it is managed by the user mode kernel32.dll. In kernel, a file is always
  opened relative to another file_object or as an absolute path. All the current
  working directory logic is done in user mode.
  """
  class Context(ApiBase.Context):
    """Processes a ETW log line and keeps the list of existent and non
    existent files accessed.

    Ignores directories.
    """
    # These indexes are for the stripped version in json.
    EVENT_NAME = 0
    TYPE = 1
    PID = 2
    TID = 3
    PROCESSOR_ID = 4
    TIMESTAMP = 5
    USER_DATA = 6

    class Process(ApiBase.Context.Process):
      def __init__(self, *args):
        super(LogmanTrace.Context.Process, self).__init__(*args)
        # Handle file objects that succeeded.
        self.file_objects = {}

    def __init__(self, blacklist, thunk_pid, trace_name, thunk_cmd):
      logging.info(
          '%s(%d, %s, %s)', self.__class__.__name__, thunk_pid, trace_name,
          thunk_cmd)
      super(LogmanTrace.Context, self).__init__(blacklist)
      self._drive_map = file_path.DosDriveMap()
      # Threads mapping to the corresponding process id.
      self._threads_active = {}
      # Process ID of the tracer, e.g. the temporary script created by
      # create_subprocess_thunk(). This is tricky because the process id may
      # have been reused.
      self._thunk_pid = thunk_pid
      self._thunk_cmd = thunk_cmd
      self._trace_name = trace_name
      self._line_number = 0
      self._thunk_process = None

    def on_line(self, line):
      """Processes a json Event line."""
      self._line_number += 1
      try:
        # By Opcode
        handler = getattr(
            self,
            'handle_%s_%s' % (line[self.EVENT_NAME], line[self.TYPE]),
            None)
        if not handler:
          raise TracingFailure(
              'Unexpected event %s_%s' % (
                  line[self.EVENT_NAME], line[self.TYPE]),
              None, None, None)
        handler(line)
      except TracingFailure, e:
        # Hack in the values since the handler could be a static function.
        e.pid = line[self.PID]
        e.line = line
        e.line_number = self._line_number
        # Re-raise the modified exception.
        raise
      except (KeyError, NotImplementedError, ValueError), e:
        raise TracingFailure(
            'Trace generated a %s exception: %s' % (
                e.__class__.__name__, str(e)),
            line[self.PID],
            self._line_number,
            line,
            e)

    def to_results(self):
      if not self.root_process:
        raise TracingFailure(
            'Failed to detect the initial process %d' % self._thunk_pid,
            None, None, None)
      process = self.root_process.to_results_process()
      return Results(process)

    def _thread_to_process(self, tid):
      """Finds the process from the thread id."""
      tid = int(tid, 16)
      pid = self._threads_active.get(tid)
      if not pid or not self._process_lookup.get(pid):
        return
      return self._process_lookup[pid]

    @classmethod
    def handle_EventTrace_Header(cls, line):
      """Verifies no event was dropped, e.g. no buffer overrun occured."""
      BUFFER_SIZE = cls.USER_DATA
      #VERSION = cls.USER_DATA + 1
      #PROVIDER_VERSION = cls.USER_DATA + 2
      #NUMBER_OF_PROCESSORS = cls.USER_DATA + 3
      #END_TIME = cls.USER_DATA + 4
      #TIMER_RESOLUTION = cls.USER_DATA + 5
      #MAX_FILE_SIZE = cls.USER_DATA + 6
      #LOG_FILE_MODE = cls.USER_DATA + 7
      #BUFFERS_WRITTEN = cls.USER_DATA + 8
      #START_BUFFERS = cls.USER_DATA + 9
      #POINTER_SIZE = cls.USER_DATA + 10
      EVENTS_LOST = cls.USER_DATA + 11
      #CPU_SPEED = cls.USER_DATA + 12
      #LOGGER_NAME = cls.USER_DATA + 13
      #LOG_FILE_NAME = cls.USER_DATA + 14
      #BOOT_TIME = cls.USER_DATA + 15
      #PERF_FREQ = cls.USER_DATA + 16
      #START_TIME = cls.USER_DATA + 17
      #RESERVED_FLAGS = cls.USER_DATA + 18
      #BUFFERS_LOST = cls.USER_DATA + 19
      #SESSION_NAME_STRING = cls.USER_DATA + 20
      #LOG_FILE_NAME_STRING = cls.USER_DATA + 21
      if line[EVENTS_LOST] != '0':
        raise TracingFailure(
            ( '%s events were lost during trace, please increase the buffer '
              'size from %s') % (line[EVENTS_LOST], line[BUFFER_SIZE]),
            None, None, None)

    def handle_FileIo_Cleanup(self, line):
      """General wisdom: if a file is closed, it's because it was opened.

      Note that FileIo_Close is not used since if a file was opened properly but
      not closed before the process exits, only Cleanup will be logged.
      """
      #IRP = self.USER_DATA
      TTID = self.USER_DATA + 1  # Thread ID, that's what we want.
      FILE_OBJECT = self.USER_DATA + 2
      #FILE_KEY = self.USER_DATA + 3
      proc = self._thread_to_process(line[TTID])
      if not proc:
        # Not a process we care about.
        return
      file_object = line[FILE_OBJECT]
      if file_object in proc.file_objects:
        filepath, access_type = proc.file_objects.pop(file_object)
        proc.add_file(filepath, access_type)

    def handle_FileIo_Create(self, line):
      """Handles a file open.

      All FileIo events are described at
      http://msdn.microsoft.com/library/windows/desktop/aa363884.aspx
      for some value of 'description'.

      " (..) process and thread id values of the IO events (..) are not valid "
      http://msdn.microsoft.com/magazine/ee358703.aspx

      The FileIo.Create event doesn't return if the CreateFile() call
      succeeded, so keep track of the file_object and check that it is
      eventually closed with FileIo_Cleanup.
      """
      #IRP = self.USER_DATA
      TTID = self.USER_DATA + 1  # Thread ID, that's what we want.
      FILE_OBJECT = self.USER_DATA + 2
      #CREATE_OPTIONS = self.USER_DATA + 3
      #FILE_ATTRIBUTES = self.USER_DATA + 4
      #SHARE_ACCESS = self.USER_DATA + 5
      OPEN_PATH = self.USER_DATA + 6

      proc = self._thread_to_process(line[TTID])
      if not proc:
        # Not a process we care about.
        return

      raw_path = line[OPEN_PATH]
      # Ignore directories and bare drive right away.
      if raw_path.endswith(os.path.sep):
        return
      filepath = self._drive_map.to_win32(raw_path)
      # Ignore bare drive right away. Some may still fall through with format
      # like '\\?\X:'
      if len(filepath) == 2:
        return
      file_object = line[FILE_OBJECT]
      if fs.isdir(filepath):
        # There is no O_DIRECTORY equivalent on Windows. The closed is
        # FILE_FLAG_BACKUP_SEMANTICS but it's not exactly right either. So
        # simply discard directories are they are found.
        return
      # Override any stale file object.
      # TODO(maruel): Figure out a way to detect if the file was opened for
      # reading or writting. Sadly CREATE_OPTIONS doesn't seem to be of any help
      # here. For now mark as None to make it clear we have no idea what it is
      # about.
      proc.file_objects[file_object] = (filepath, None)

    def handle_FileIo_Rename(self, line):
      # TODO(maruel): Handle?
      pass

    def handle_Process_End(self, line):
      pid = line[self.PID]
      if self._process_lookup.get(pid):
        logging.info('Terminated: %d' % pid)
        self._process_lookup[pid] = None
      else:
        logging.debug('Terminated: %d' % pid)
      if self._thunk_process and self._thunk_process.pid == pid:
        self._thunk_process = None

    def handle_Process_Start(self, line):
      """Handles a new child process started by PID."""
      #UNIQUE_PROCESS_KEY = self.USER_DATA
      PROCESS_ID = self.USER_DATA + 1
      #PARENT_PID = self.USER_DATA + 2
      #SESSION_ID = self.USER_DATA + 3
      #EXIT_STATUS = self.USER_DATA + 4
      #DIRECTORY_TABLE_BASE = self.USER_DATA + 5
      #USER_SID = self.USER_DATA + 6
      IMAGE_FILE_NAME = self.USER_DATA + 7
      COMMAND_LINE = self.USER_DATA + 8

      ppid = line[self.PID]
      pid = int(line[PROCESS_ID], 16)
      command_line = CommandLineToArgvW(line[COMMAND_LINE])
      logging.debug(
          'New process %d->%d (%s) %s' %
            (ppid, pid, line[IMAGE_FILE_NAME], command_line))

      if pid == self._thunk_pid:
        # Need to ignore processes we don't know about because the log is
        # system-wide. self._thunk_pid shall start only one process.
        # This is tricky though because Windows *loves* to reuse process id and
        # it happens often that the process ID of the thunk script created by
        # create_subprocess_thunk() is reused. So just detecting the pid here is
        # not sufficient, we must confirm the command line.
        if command_line[:len(self._thunk_cmd)] != self._thunk_cmd:
          logging.info(
              'Ignoring duplicate pid %d for %s: %s while searching for %s',
              pid, self._trace_name, command_line, self._thunk_cmd)
          return

        # TODO(maruel): The check is quite weak. Add the thunk path.
        if self._thunk_process:
          raise TracingFailure(
              ( 'Parent process is _thunk_pid(%d) but thunk_process(%d) is '
                'already set') % (self._thunk_pid, self._thunk_process.pid),
              None, None, None)
        proc = self.Process(self.blacklist, pid, None)
        self._thunk_process = proc
        return
      elif ppid == self._thunk_pid and self._thunk_process:
        proc = self.Process(self.blacklist, pid, None)
        self.root_process = proc
        ppid = None
      elif self._process_lookup.get(ppid):
        proc = self.Process(self.blacklist, pid, None)
        self._process_lookup[ppid].children.append(proc)
      else:
        # Ignore
        return
      self._process_lookup[pid] = proc

      proc.command = command_line
      proc.executable = line[IMAGE_FILE_NAME]
      # proc.command[0] may be the absolute path of 'executable' but it may be
      # anything else too. If it happens that command[0] ends with executable,
      # use it, otherwise defaults to the base name.
      cmd0 = proc.command[0].lower()
      if not cmd0.endswith('.exe'):
        # TODO(maruel): That's not strictly true either.
        cmd0 += '.exe'
      if cmd0.endswith(proc.executable) and fs.isfile(cmd0):
        # Fix the path.
        cmd0 = cmd0.replace('/', os.path.sep)
        cmd0 = os.path.normpath(cmd0)
        proc.executable = file_path.get_native_path_case(cmd0)
      logging.info(
          'New child: %s -> %d %s' % (ppid, pid, proc.executable))

    def handle_Thread_End(self, line):
      """Has the same parameters as Thread_Start."""
      tid = int(line[self.TID], 16)
      self._threads_active.pop(tid, None)

    def handle_Thread_Start(self, line):
      """Handles a new thread created.

      Do not use self.PID here since a process' initial thread is created by
      the parent process.
      """
      PROCESS_ID = self.USER_DATA
      TTHREAD_ID = self.USER_DATA + 1
      #STACK_BASE = self.USER_DATA + 2
      #STACK_LIMIT = self.USER_DATA + 3
      #USER_STACK_BASE = self.USER_DATA + 4
      #USER_STACK_LIMIT = self.USER_DATA + 5
      #AFFINITY = self.USER_DATA + 6
      #WIN32_START_ADDR = self.USER_DATA + 7
      #TEB_BASE = self.USER_DATA + 8
      #SUB_PROCESS_TAG = self.USER_DATA + 9
      #BASE_PRIORITY = self.USER_DATA + 10
      #PAGE_PRIORITY = self.USER_DATA + 11
      #IO_PRIORITY = self.USER_DATA + 12
      #THREAD_FLAGS = self.USER_DATA + 13
      # Do not use self.PID here since a process' initial thread is created by
      # the parent process.
      pid = int(line[PROCESS_ID], 16)
      tid = int(line[TTHREAD_ID], 16)
      logging.debug('New thread pid:%d, tid:%d' % (pid, tid))
      self._threads_active[tid] = pid

    @classmethod
    def supported_events(cls):
      """Returns all the procesed events."""
      out = []
      for member in dir(cls):
        match = re.match(r'^handle_([A-Za-z]+)_([A-Za-z]+)$', member)
        if match:
          out.append(match.groups())
      return out

  class Tracer(ApiBase.Tracer):
    # The basic headers.
    EXPECTED_HEADER = [
      u'Event Name',
      u'Type',
      u'Event ID',
      u'Version',
      u'Channel',
      u'Level',  # 5
      u'Opcode',
      u'Task',
      u'Keyword',
      u'PID',
      u'TID',  # 10
      u'Processor Number',
      u'Instance ID',
      u'Parent Instance ID',
      u'Activity ID',
      u'Related Activity ID',  # 15
      u'Clock-Time',
      u'Kernel(ms)',  # Both have a resolution of ~15ms which makes them
      u'User(ms)',    # pretty much useless.
      u'User Data',   # Extra arguments that are event-specific.
    ]
    # Only the useful headers common to all entries are listed there. Any column
    # at 19 or higher is dependent on the specific event.
    EVENT_NAME = 0
    TYPE = 1
    PID = 9
    TID = 10
    PROCESSOR_ID = 11
    TIMESTAMP = 16
    NULL_GUID = '{00000000-0000-0000-0000-000000000000}'
    USER_DATA = 19

    class CsvReader(object):
      """CSV reader that reads files generated by tracerpt.exe.

      csv.reader() fails to read them properly, it mangles file names quoted
      with "" with a comma in it.
      """
        # 0. Had a ',' or one of the following ' ' after a comma, next should
        # be ' ', '"' or string or ',' for an empty field.
      ( HAD_DELIMITER,
        # 1. Processing an unquoted field up to ','.
        IN_STR,
        # 2. Processing a new field starting with '"'.
        STARTING_STR_QUOTED,
        # 3. Second quote in a row at the start of a field. It could be either
        # '""foo""' or '""'. Who the hell thought it was a great idea to use
        # the same character for delimiting and escaping?
        STARTING_SECOND_QUOTE,
        # 4. A quote inside a quoted string where the previous character was
        # not a quote, so the string is not empty. Can be either: end of a
        # quoted string (a delimiter) or a quote escape. The next char must be
        # either '"' or ','.
        HAD_QUOTE_IN_QUOTED,
        # 5. Second quote inside a quoted string.
        HAD_SECOND_QUOTE_IN_A_ROW_IN_QUOTED,
        # 6. Processing a field that started with '"'.
        IN_STR_QUOTED) = range(7)

      def __init__(self, f):
        self.f = f

      def __iter__(self):
        return self

      def next(self):
        """Splits the line in fields."""
        line = self.f.readline()
        if not line:
          raise StopIteration()
        line = line.strip()
        fields = []
        state = self.HAD_DELIMITER
        for i, c in enumerate(line):
          if state == self.HAD_DELIMITER:
            if c == ',':
              # Empty field.
              fields.append('')
            elif c == ' ':
              # Ignore initial whitespaces
              pass
            elif c == '"':
              state = self.STARTING_STR_QUOTED
              fields.append('')
            else:
              # Start of a new field.
              state = self.IN_STR
              fields.append(c)

          elif state == self.IN_STR:
            # Do not accept quote inside unquoted field.
            assert c != '"', (i, c, line, fields)
            if c == ',':
              fields[-1] = fields[-1].strip()
              state = self.HAD_DELIMITER
            else:
              fields[-1] = fields[-1] + c

          elif state == self.STARTING_STR_QUOTED:
            if c == '"':
              # Do not store the character yet.
              state = self.STARTING_SECOND_QUOTE
            else:
              state = self.IN_STR_QUOTED
              fields[-1] = fields[-1] + c

          elif state == self.STARTING_SECOND_QUOTE:
            if c == ',':
              # It was an empty field. '""' == ''.
              state = self.HAD_DELIMITER
            else:
              fields[-1] = fields[-1] + '"' + c
              state = self.IN_STR_QUOTED

          elif state == self.HAD_QUOTE_IN_QUOTED:
            if c == ',':
              # End of the string.
              state = self.HAD_DELIMITER
            elif c == '"':
              state = self.HAD_SECOND_QUOTE_IN_A_ROW_IN_QUOTED
            else:
              # The previous double-quote was just an unescaped quote.
              fields[-1] = fields[-1] + '"' + c
              state = self.IN_STR_QUOTED

          elif state == self.HAD_SECOND_QUOTE_IN_A_ROW_IN_QUOTED:
            if c == ',':
              # End of the string.
              state = self.HAD_DELIMITER
              fields[-1] = fields[-1] + '"'
            else:
              # That's just how the logger rolls. Revert back to appending the
              # char and "guess" it was a quote in a double-quoted string.
              state = self.IN_STR_QUOTED
              fields[-1] = fields[-1] + '"' + c

          elif state == self.IN_STR_QUOTED:
            if c == '"':
              # Could be a delimiter or an escape.
              state = self.HAD_QUOTE_IN_QUOTED
            else:
              fields[-1] = fields[-1] + c

        if state == self.HAD_SECOND_QUOTE_IN_A_ROW_IN_QUOTED:
          fields[-1] = fields[-1] + '"'
        else:
          assert state in (
              # Terminated with a normal field.
              self.IN_STR,
              # Terminated with an empty field.
              self.STARTING_SECOND_QUOTE,
              # Terminated with a normal quoted field.
              self.HAD_QUOTE_IN_QUOTED), (
              line, state, fields)
        return fields

    def __init__(self, logname):
      """Starts the log collection.

      Requires administrative access. logman.exe is synchronous so no need for a
      "warmup" call.  'Windows Kernel Trace' is *localized* so use its GUID
      instead.  The GUID constant name is SystemTraceControlGuid. Lovely.

      One can get the list of potentially interesting providers with:
      "logman query providers | findstr /i file"
      """
      super(LogmanTrace.Tracer, self).__init__(logname)
      self._signal_script = create_subprocess_thunk()
      self._scripts_to_cleanup.append(self._signal_script)
      cmd_start = [
        'logman.exe',
        'start',
        'NT Kernel Logger',
        '-p', '{9e814aad-3204-11d2-9a82-006008a86939}',
        # splitio,fileiocompletion,syscall,file,cswitch,img
        '(process,fileio,thread)',
        '-o', self._logname + '.etl',
        '-ets',  # Send directly to kernel
        # Values extracted out of thin air.
        # Event Trace Session buffer size in kb.
        '-bs', '10240',
        # Number of Event Trace Session buffers.
        '-nb', '16', '256',
      ]
      logging.debug('Running: %s' % cmd_start)
      try:
        subprocess.check_call(
            cmd_start,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT)
      except subprocess.CalledProcessError, e:
        if e.returncode == -2147024891:
          print >> sys.stderr, 'Please restart with an elevated admin prompt'
        elif e.returncode == -2144337737:
          print >> sys.stderr, (
              'A kernel trace was already running, stop it and try again')
        raise

    def trace(self, cmd, cwd, tracename, output):
      logging.info('trace(%s, %s, %s, %s)' % (cmd, cwd, tracename, output))
      assert os.path.isabs(cmd[0]), cmd[0]
      assert os.path.isabs(cwd), cwd
      assert os.path.normpath(cwd) == cwd, cwd
      with self._lock:
        if not self._initialized:
          raise TracingFailure(
              'Called Tracer.trace() on an unitialized object',
              None, None, None, tracename)
        assert tracename not in (i['trace'] for i in self._traces)

      # Use "logman -?" for help.

      stdout = stderr = None
      if output:
        stdout = subprocess.PIPE
        stderr = subprocess.STDOUT

      # Run the child process.
      logging.debug('Running: %s' % cmd)
      # Use the temporary script generated with create_subprocess_thunk() so we
      # have a clear pid owner. Since trace_inputs.py can be used as a library
      # and could trace multiple processes simultaneously, it makes it more
      # complex if the executable to be traced is executed directly here. It
      # also solves issues related to logman.exe that needs to be executed to
      # control the kernel trace.
      child_cmd = [
        sys.executable,
        self._signal_script,
        tracename,
      ]
      child = subprocess.Popen(
          child_cmd + tools.fix_python_path(cmd),
          cwd=cwd,
          stdin=subprocess.PIPE,
          stdout=stdout,
          stderr=stderr)
      logging.debug('Started child pid: %d' % child.pid)
      out = child.communicate()[0]
      # This doesn't mean all the grand-children are done. Sadly, we don't have
      # a good way to determine that.

      with self._lock:
        assert tracename not in (i['trace'] for i in self._traces)
        self._traces.append({
          'cmd': cmd,
          'cwd': cwd,
          'output': out,
          'pid': child.pid,
          # Used to figure out the real process when process ids are reused.
          'thunk_cmd': child_cmd,
          'trace': tracename,
        })

      return child.returncode, out

    def close(self, _timeout=None):
      """Stops the kernel log collection and converts the traces to text
      representation.
      """
      with self._lock:
        try:
          super(LogmanTrace.Tracer, self).close()
        finally:
          cmd_stop = [
            'logman.exe',
            'stop',
            'NT Kernel Logger',
            '-ets',  # Sends the command directly to the kernel.
          ]
          logging.debug('Running: %s' % cmd_stop)
          subprocess.check_call(
              cmd_stop,
              stdin=subprocess.PIPE,
              stdout=subprocess.PIPE,
              stderr=subprocess.STDOUT)

    def post_process_log(self):
      """Converts the .etl file into .csv then into .json."""
      super(LogmanTrace.Tracer, self).post_process_log()
      logformat = 'csv'
      self._convert_log(logformat)
      self._trim_log(logformat)

    def _gen_logdata(self):
      return  {
        'format': 'csv',
        'traces': self._traces,
      }

    def _trim_log(self, logformat):
      """Reduces the amount of data in original log by generating a 'reduced'
      log.
      """
      if logformat == 'csv_utf16':
        file_handle = codecs.open(
            self._logname + '.' + logformat, 'r', encoding='utf-16')

      elif logformat == 'csv':
        assert sys.getfilesystemencoding() == 'mbcs'
        file_handle = codecs.open(
              self._logname + '.' + logformat, 'r',
              encoding=get_current_encoding())

      supported_events = LogmanTrace.Context.supported_events()

      def trim(generator):
        """Loads items from the generator and returns the interesting data.

        It filters out any uninteresting line and reduce the amount of data in
        the trace.
        """
        for index, line in enumerate(generator):
          if not index:
            if line != self.EXPECTED_HEADER:
              raise TracingFailure(
                  'Found malformed header: %s' % line,
                  None, None, None)
            continue
          # As you can see, the CSV is full of useful non-redundant information:
          if (line[2] != '0' or  # Event ID
              line[3] not in ('2', '3') or  # Version
              line[4] != '0' or  # Channel
              line[5] != '0' or  # Level
              line[7] != '0' or  # Task
              line[8] != '0x0000000000000000' or  # Keyword
              line[12] != '' or  # Instance ID
              line[13] != '' or  # Parent Instance ID
              line[14] != self.NULL_GUID or  # Activity ID
              line[15] != ''):  # Related Activity ID
            raise TracingFailure(
                'Found unexpected values in line: %s' % ' '.join(line),
                  None, None, None)

          if (line[self.EVENT_NAME], line[self.TYPE]) not in supported_events:
            continue

          yield [
              line[self.EVENT_NAME],
              line[self.TYPE],
              line[self.PID],
              line[self.TID],
              line[self.PROCESSOR_ID],
              line[self.TIMESTAMP],
          ] + line[self.USER_DATA:]

      # must not convert the trim() call into a list, since it will use too much
      # memory for large trace. use a csv file as a workaround since the json
      # parser requires a complete in-memory file.
      path = os.path.join(
          unicode(os.getcwd()), u'%s.preprocessed' % self._logname)
      with fs.open(path, 'wb') as f:
        # $ and * can't be used in file name on windows, reducing the likelihood
        # of having to escape a string.
        out = csv.writer(
            f, delimiter='$', quotechar='*', quoting=csv.QUOTE_MINIMAL)
        for line in trim(self.CsvReader(file_handle)):
          out.writerow([s.encode('utf-8') for s in line])

    def _convert_log(self, logformat):
      """Converts the ETL trace to text representation.

      Normally, 'csv' is sufficient. If complex scripts are used (like eastern
      languages), use 'csv_utf16'. If localization gets in the way, use 'xml'.

      Arguments:
        - logformat: Text format to be generated, csv, csv_utf16 or xml.

      Use "tracerpt -?" for help.
      """
      LOCALE_INVARIANT = 0x7F
      windll.kernel32.SetThreadLocale(LOCALE_INVARIANT)
      cmd_convert = [
        'tracerpt.exe',
        '-l', self._logname + '.etl',
        '-o', self._logname + '.' + logformat,
        '-gmt',  # Use UTC
        '-y',  # No prompt
        # Use -of XML to get the header of each items after column 19, e.g. all
        # the actual headers of 'User Data'.
      ]

      if logformat == 'csv':
        # tracerpt localizes the 'Type' column, for major brainfuck
        # entertainment. I can't imagine any sane reason to do that.
        cmd_convert.extend(['-of', 'CSV'])
      elif logformat == 'csv_utf16':
        # This causes it to use UTF-16, which doubles the log size but ensures
        # the log is readable for non-ASCII characters.
        cmd_convert.extend(['-of', 'CSV', '-en', 'Unicode'])
      elif logformat == 'xml':
        cmd_convert.extend(['-of', 'XML'])
      else:
        raise ValueError('Unexpected log format \'%s\'' % logformat)
      logging.debug('Running: %s' % cmd_convert)
      # This can takes tens of minutes for large logs.
      # Redirects all output to stderr.
      subprocess.check_call(
          cmd_convert,
          stdin=subprocess.PIPE,
          stdout=sys.stderr,
          stderr=sys.stderr)

  def __init__(self, use_sudo=False):  # pylint: disable=W0613
    super(LogmanTrace, self).__init__()
    # Ignore use_sudo. It's irrelevant on Windows but kept to simplify the API.

  @staticmethod
  def clean_trace(logname):
    for ext in ('', '.csv', '.etl', '.json', '.xml', '.preprocessed'):
      if fs.isfile(logname + ext):
        fs.remove(logname + ext)

  @classmethod
  def parse_log(cls, logname, blacklist, trace_name):
    logging.info('parse_log(%s, ..., %s)', logname, trace_name)
    assert os.path.isabs(logname)

    def blacklist_more(filepath):
      # All the NTFS metadata is in the form x:\$EXTEND or stuff like that.
      return blacklist(filepath) or re.match(r'[A-Z]\:\\\$EXTEND', filepath)

    # Create a list of (Context, result_dict) tuples. This is necessary because
    # the csv file may be larger than the amount of available memory.
    contexes = [
      (
        cls.Context(
            blacklist_more, item['pid'], item['trace'], item['thunk_cmd']),
        {
          'output': item['output'],
          'trace': item['trace'],
        },
      )
      for item in tools.read_json(logname)['traces']
      if not trace_name or item['trace'] == trace_name
    ]

    # The log may be too large to fit in memory and it is not efficient to read
    # it multiple times, so multiplex the contexes instead, which is slightly
    # more awkward.
    path = os.path.join(unicode(os.getcwd()), u'%s.preprocessed' % logname)
    with fs.open(path, 'rb') as f:
      lines = csv.reader(
          f, delimiter='$', quotechar='*', quoting=csv.QUOTE_MINIMAL)
      for encoded in lines:
        line = [s.decode('utf-8') for s in encoded]
        # Convert the PID in-place from hex.
        line[cls.Context.PID] = int(line[cls.Context.PID], 16)
        for context in contexes:
          if 'exception' in context[1]:
            continue
          try:
            context[0].on_line(line)
          except TracingFailure:
            context[1]['exception'] = sys.exc_info()

    for context in contexes:
      if 'exception' in context[1]:
        continue
      context[1]['results'] = context[0].to_results()

    return [context[1] for context in contexes]


def get_api(**kwargs):
  """Returns the correct implementation for the current OS."""
  if sys.platform == 'cygwin':
    raise NotImplementedError(
        'Not implemented for cygwin, start the script from Win32 python')
  flavors = {
    'win32': LogmanTrace,
    'darwin': Dtrace,
    'sunos5': Dtrace,
    'freebsd7': Dtrace,
    'freebsd8': Dtrace,
  }
  # Defaults to strace.
  return flavors.get(sys.platform, Strace)(**kwargs)


def extract_directories(root_dir, files, blacklist):
  """Detects if all the files in a directory are in |files| and if so, replace
  the individual files by a Results.Directory instance.

  Takes a list of Results.File instances and returns a shorter list of
  Results.File and Results.Directory instances.

  Arguments:
    - root_dir: Optional base directory that shouldn't be search further.
    - files: list of Results.File instances.
    - blacklist: lambda to reject unneeded files, for example r'.+\.pyc'.
  """
  logging.info(
      'extract_directories(%s, %d files, ...)' % (root_dir, len(files)))
  assert not (root_dir or '').endswith(os.path.sep), root_dir
  # It is important for root_dir to not be a symlinked path, make sure to call
  # os.path.realpath() as needed.
  assert not root_dir or (
      fs.realpath(file_path.get_native_path_case(root_dir)) == root_dir)
  assert not any(isinstance(f, Results.Directory) for f in files)
  # Remove non existent files.
  files = [f for f in files if f.existent]
  if not files:
    return files
  # All files must share the same root, which can be None.
  assert len(set(f.root for f in files)) == 1, set(f.root for f in files)

  # Creates a {directory: {filename: File}} mapping, up to root.
  buckets = {}
  if root_dir:
    buckets[root_dir] = {}
  for fileobj in files:
    path = fileobj.full_path
    directory = os.path.dirname(path)
    assert directory
    # Do not use os.path.basename() so trailing os.path.sep is kept.
    basename = path[len(directory)+1:]
    files_in_directory = buckets.setdefault(directory, {})
    files_in_directory[basename] = fileobj
    # Add all the directories recursively up to root.
    while True:
      old_d = directory
      directory = os.path.dirname(directory)
      if directory + os.path.sep == root_dir or directory == old_d:
        break
      buckets.setdefault(directory, {})

  root_prefix = len(root_dir) + 1 if root_dir else 0
  for directory in sorted(buckets, reverse=True):
    if not fs.isdir(directory):
      logging.debug(
          '%s was a directory but doesn\'t exist anymore; ignoring', directory)
      continue
    actual = set(f for f in fs.listdir(directory) if not blacklist(f))
    expected = set(buckets[directory])
    if not (actual - expected):
      parent = os.path.dirname(directory)
      buckets[parent][os.path.basename(directory)] = Results.Directory(
        root_dir,
        directory[root_prefix:],
        False,
        sum(f.size for f in buckets[directory].itervalues()),
        sum(f.nb_files for f in buckets[directory].itervalues()))
      # Remove the whole bucket.
      del buckets[directory]

  # Reverse the mapping with what remains. The original instances are returned,
  # so the cached meta data is kept.
  files = sum((x.values() for x in buckets.itervalues()), [])
  return sorted(files, key=lambda x: x.path)


def trace(logfile, cmd, cwd, api, output):
  """Traces an executable. Returns (returncode, output) from api.

  Arguments:
  - logfile: file to write to.
  - cmd: command to run.
  - cwd: current directory to start the process in.
  - api: a tracing api instance.
  - output: if True, returns output, otherwise prints it at the console.
  """
  cmd = tools.fix_python_path(cmd)
  api.clean_trace(logfile)
  with api.get_tracer(logfile) as tracer:
    return tracer.trace(cmd, cwd, 'default', output)


def CMDclean(parser, args):
  """Cleans up traces."""
  options, args = parser.parse_args(args)
  api = get_api()
  api.clean_trace(options.log)
  return 0


def CMDtrace(parser, args):
  """Traces an executable."""
  parser.allow_interspersed_args = False
  parser.add_option(
      '-q', '--quiet', action='store_true',
      help='Redirects traced executable output to /dev/null')
  parser.add_option(
      '-s', '--sudo', action='store_true',
      help='Use sudo when shelling out the tracer tool (ignored on Windows)')
  parser.add_option(
      '-n', '--no-sudo', action='store_false',
      help='Don\'t use sudo')
  options, args = parser.parse_args(args)

  if not args:
    parser.error('Please provide a command to run')

  if not can_trace():
    parser.error('Please rerun this program with admin privileges')

  if not os.path.isabs(args[0]) and fs.access(args[0], os.X_OK):
    args[0] = os.path.abspath(args[0])

  # options.sudo default value is None, which is to do whatever tracer defaults
  # do.
  api = get_api(use_sudo=options.sudo)
  return trace(options.log, args, os.getcwd(), api, options.quiet)[0]


def CMDread(parser, args):
  """Reads the logs and prints the result."""
  parser.add_option(
      '-V', '--variable',
      nargs=2,
      action='append',
      dest='variables',
      metavar='VAR_NAME directory',
      default=[],
      help=('Variables to replace relative directories against. Example: '
            '"-v \'$HOME\' \'/home/%s\'" will replace all occurence of your '
            'home dir with $HOME') % getpass.getuser())
  parser.add_option(
      '--root-dir',
      help='Root directory to base everything off it. Anything outside of this '
           'this directory will not be reported')
  parser.add_option(
      '--trace-name',
      help='Only reads one of the trace. Defaults to reading all traces')
  parser.add_option(
      '-j', '--json', action='store_true',
      help='Outputs raw result data as json')
  parser.add_option(
      '--trace-blacklist', action='append', default=[],
      help='List of regexp to use as blacklist filter')
  options, args = parser.parse_args(args)

  if options.root_dir:
    options.root_dir = file_path.get_native_path_case(
        unicode(os.path.abspath(options.root_dir)))

  variables = dict(options.variables)
  api = get_api()
  blacklist = tools.gen_blacklist(options.trace_blacklist)
  data = api.parse_log(options.log, blacklist, options.trace_name)
  # Process each trace.
  output_as_json = []
  try:
    for item in data:
      if 'exception' in item:
        # Do not abort the other traces.
        print >> sys.stderr, (
            'Trace %s: Got an exception: %s' % (
              item['trace'], item['exception'][1]))
        continue
      results = item['results']
      if options.root_dir:
        results = results.strip_root(options.root_dir)

      if options.json:
        output_as_json.append(results.flatten())
      else:
        simplified = extract_directories(
            options.root_dir, results.files, blacklist)
        simplified = [f.replace_variables(variables) for f in simplified]
        if len(data) > 1:
          print('Trace: %s' % item['trace'])
        print('Total: %d' % len(results.files))
        print('Non existent: %d' % len(results.non_existent))
        for f in results.non_existent:
          print('  %s' % f.path)
        print(
            'Interesting: %d reduced to %d' % (
                len(results.existent), len(simplified)))
        for f in simplified:
          print('  %s' % f.path)

    if options.json:
      tools.write_json(sys.stdout, output_as_json, False)
  except KeyboardInterrupt:
    return 1
  except IOError as e:
    if e.errno == errno.EPIPE:
      # Do not print a stack trace when the output is piped to less and the user
      # quits before the whole output was written.
      return 1
    raise
  return 0


class OptionParserTraceInputs(logging_utils.OptionParserWithLogging):
  """Adds automatic --log handling."""

  # Disable --log-file options since both --log and --log-file options are
  # confusing.
  # TODO(vadimsh): Rename --log-file or --log to something else.
  enable_log_file = False

  def __init__(self, **kwargs):
    logging_utils.OptionParserWithLogging.__init__(self, **kwargs)
    self.add_option(
        '-l', '--log', help='Log file to generate or read, required')

  def parse_args(self, *args, **kwargs):
    """Makes sure the paths make sense.

    On Windows, / and \ are often mixed together in a path.
    """
    options, args = logging_utils.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    if not options.log:
      self.error('Must supply a log file with -l')
    options.log = unicode(os.path.abspath(options.log))
    return options, args


def main(argv):
  dispatcher = subcommand.CommandDispatcher(__name__)
  try:
    return dispatcher.execute(
        OptionParserTraceInputs(version=__version__), argv)
  except TracingFailure, e:
    sys.stderr.write('\nError: ')
    sys.stderr.write(str(e))
    sys.stderr.write('\n')
    return 1


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
