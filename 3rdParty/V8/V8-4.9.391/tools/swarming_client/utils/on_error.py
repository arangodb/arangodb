# coding=utf-8
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Declares a single function to report errors to a server.

By running the script, you accept that errors will be reported to the server you
connect to.
"""

import atexit
import getpass
import os
import platform
import re
import socket
import sys
import time
import traceback

from . import net
from . import tools
from . import zip_package


# It is very important to not get reports from non Chromium infrastructure. We
# *really* do not want to know anything about you, dear non Google employee.
_ENABLED_DOMAINS = (
  '.chromium.org',
  '.google.com',
  '.google.com.internal',
)

# If this envar is '1' then disable reports. Useful when developing the client.
_DISABLE_ENVVAR = 'SWARMING_DISABLE_ON_ERROR'


# Set this variable to the net.HttpService server to be used to report errors.
# It must be done early at process startup. Once this value is set, it is
# considered that the atexit handler is enabled.
_SERVER = None

# This is tricky because it is looked at during import time. At atexit time,
# __file__ is not defined anymore so it has to be saved first. Also make it work
# when executed directly from a .zip file. Also handle interactive mode where
# this is always set to None.
_SOURCE = zip_package.get_main_script_path()
if _SOURCE:
  _SOURCE = os.path.basename(_SOURCE)

_TIME_STARTED = time.time()

_HOSTNAME = None


# Paths that can be stripped from the stack traces by _relative_path().
_PATHS_TO_STRIP = (
  os.getcwd() + os.path.sep,
  os.path.dirname(os.__file__) + os.path.sep,
  '.' + os.path.sep,
)


# Used to simplify the stack trace, by removing path information when possible.
_RE_STACK_TRACE_FILE = (
    r'^(?P<prefix>  File \")(?P<file>[^\"]+)(?P<suffix>\"\, line )'
    r'(?P<line_no>\d+)(?P<rest>|\, in .+)$')


### Private stuff.


def _relative_path(path):
  """Strips the current working directory or common library prefix.

  Used by Formatter.
  """
  for i in _PATHS_TO_STRIP:
    if path.startswith(i):
      return path[len(i):]
  return path


def _reformat_stack(stack):
  """Post processes the stack trace through _relative_path()."""
  def replace(l):
    m = re.match(_RE_STACK_TRACE_FILE, l, re.DOTALL)
    if m:
      groups = list(m.groups())
      groups[1] = _relative_path(groups[1])
      return ''.join(groups)
    return l

  # Trim paths.
  out = map(replace, stack.splitlines(True))

  # Trim indentation.
  while all(l.startswith(' ') for l in out):
    out = [l[1:] for l in out]
  return ''.join(out)


def _format_exception(e):
  """Returns a human readable form of an exception.

  Adds the maximum number of interesting information in the safest way."""
  try:
    out = repr(e)
  except Exception:
    out = ''
  try:
    out = str(e)
  except Exception:
    pass
  return out


def _post(params):
  """Executes the HTTP Post to the server."""
  if not _SERVER:
    return None
  return _SERVER.json_request(
      '/ereporter2/api/v1/on_error', data=params, max_attempts=1, timeout=20)


def _serialize_env():
  """Makes os.environ json serializable.

  It happens that the environment variable may have non-ASCII characters like
  ANSI escape code.
  """
  return dict(
      (k, v.encode('ascii', 'replace')) for k, v in os.environ.iteritems())


def _report_exception(message, e, stack):
  """Sends the stack trace to the breakpad server."""
  name = 'crash report' if e else 'report'
  sys.stderr.write('Sending the %s ...' % name)
  message = (message or '').rstrip()
  if e:
    if message:
      message += '\n'
    message += (_format_exception(e)).rstrip()

  params = {
    'args': sys.argv,
    'cwd': os.getcwd(),
    'duration': time.time() - _TIME_STARTED,
    'env': _serialize_env(),
    'hostname': _HOSTNAME,
    'message': message,
    'os': sys.platform,
    'python_version': platform.python_version(),
    'source': _SOURCE,
    'user': getpass.getuser(),
  }
  if e:
    params['category'] = 'exception'
    params['exception_type'] = e.__class__.__name__
  else:
    params['category'] = 'report'

  if stack:
    params['stack'] = _reformat_stack(stack).rstrip()
    if len(params['stack']) > 4096:
      params['stack'] = params['stack'][:4095] + '…'

  version = getattr(sys.modules['__main__'], '__version__', None)
  if version:
    params['version'] = version

  data = {
    'r': params,
    # Bump the version when changing the packet format.
    'v': '1',
  }
  response = _post(data)
  if response and response.get('url'):
    sys.stderr.write(' done.\nReport URL: %s\n' % response['url'])
  else:
    sys.stderr.write(' failed!\n')
  sys.stderr.write(message + '\n')


def _check_for_exception_on_exit():
  """Runs at exit. Look if there was an exception active and report if so.

  Since atexit() may not be called from the frame itself, use sys.last_value.
  """
  # Sadly, sys.exc_info() cannot be used here, since atexit calls are called
  # outside the exception handler.
  exception = getattr(sys, 'last_value', None)
  if not exception or isinstance(exception, KeyboardInterrupt):
    return

  last_tb = getattr(sys, 'last_traceback', None)
  if not last_tb:
    return

  _report_exception(
      'Process exited due to exception',
      exception,
      ''.join(traceback.format_tb(last_tb)))


def _is_in_test():
  """Returns True if filename of __main__ module ends with _test.py(c)."""
  main_file = os.path.basename(getattr(sys.modules['__main__'], '__file__', ''))
  return os.path.splitext(main_file)[0].endswith('_test')


### Public API.


def report_on_exception_exit(server):
  """Registers the callback at exit to report an error if the process exits due
  to an exception.
  """
  global _HOSTNAME
  global _SERVER
  if _SERVER:
    raise ValueError('on_error.report_on_exception_exit() was called twice')

  if tools.get_bool_env_var(_DISABLE_ENVVAR):
    return False

  if _is_in_test():
    # Disable when running inside unit tests process.
    return False

  if not server.startswith('https://'):
    # Only allow report over HTTPS. Silently drop it.
    return False

  _HOSTNAME = socket.getfqdn()
  if not _HOSTNAME.endswith(_ENABLED_DOMAINS):
    # Silently skip non-google infrastructure. Technically, it reports to the
    # server the client code is talking to so in practice, it would be safe for
    # non googler to manually enable this assuming their client code talks to a
    # server they also own. Please send a CL if you desire this functionality.
    return False

  _SERVER = net.get_http_service(server, allow_cached=False)
  atexit.register(_check_for_exception_on_exit)
  return True


def report(error):
  """Either reports an error to the server or prints a error to stderr.

  It's indented to be used only for non recoverable unexpected errors that must
  be monitored at server-level like API request failure. Is should NOT be used
  for input validation, command line argument errors, etc.

  Arguments:
    error: error message string (possibly multiple lines) or None. If a
        exception frame is active, it will be logged.
  """
  exc_info = sys.exc_info()
  if _SERVER:
    _report_exception(
        error, exc_info[1], ''.join(traceback.format_tb(exc_info[2])))
    return

  if error:
    sys.stderr.write(error + '\n')
  if exc_info[1]:
    sys.stderr.write(_format_exception(exc_info[1]) + '\n')
