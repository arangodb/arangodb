# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for logging.

Example usage:

.. code-block:: python

    import argparse
    import logging
    import infra_libs.logs

    parser = argparse.ArgumentParser()
    infra_libs.logs.add_argparse_options(parser)

    options = parser.parse_args()
    infra_libs.logs.process_argparse_options(options)

    LOGGER = logging.getLogger(__name__)
    LOGGER.info('test message')

The last line should print something like::

  [I2014-06-27T11:42:32.418716-07:00 7082 logs:71] test message

"""

import datetime
import getpass
import inspect
import logging
import logging.handlers
import os
import re
import socket
import sys
import tempfile
import textwrap

import pytz

from infra_libs.ts_mon.common.metrics import CumulativeMetric

log_metric = CumulativeMetric(
  'proc/log_lines', description="Number of log lines, per severity level.")

if sys.platform.startswith('win'):  # pragma: no cover
  DEFAULT_LOG_DIRECTORIES = os.pathsep.join([
      'E:\\chrome-infra-logs',
      'C:\\chrome-infra-logs',
  ])
else:
  DEFAULT_LOG_DIRECTORIES = '/var/log/chrome-infra'


class InfraFilter(logging.Filter):  # pragma: no cover
  """Adds fields used by the infra-specific formatter.

  Fields added:

  - 'iso8601': timestamp
  - 'severity': one-letter indicator of log level (first letter of levelname).
  - 'fullModuleName': name of the module including the package name.

  Args:
    timezone (str): timezone in which timestamps should be printed.
    module_name_blacklist (str): do not print log lines from modules whose name
      matches this regular expression.
  """
  def __init__(self, timezone, module_name_blacklist=None):
    super(InfraFilter, self).__init__()
    self.module_name_blacklist = None

    if module_name_blacklist:
      self.module_name_blacklist = re.compile(module_name_blacklist)

    self.tz = pytz.timezone(timezone)

  def filter(self, record):
    dt = datetime.datetime.fromtimestamp(record.created, tz=pytz.utc)
    record.iso8601 = self.tz.normalize(dt).isoformat()
    record.severity = record.levelname[0]
    log_metric.increment(fields={'level': record.severity})
    record.fullModuleName = self._full_module_name() or record.module
    if self.module_name_blacklist:
      if self.module_name_blacklist.search(record.fullModuleName):
        return False
    return True

  def _full_module_name(self):
    frame = inspect.currentframe()
    try:
      while frame is not None:
        try:
          name = frame.f_globals['__name__']
        except KeyError:
          continue

        if name not in (__name__, 'logging'):
          return name
        frame = frame.f_back
    finally:
      del frame

    return None


class InfraFormatter(logging.Formatter):  # pragma: no cover
  """Formats log messages in a standard way.

  This object processes fields added by :class:`InfraFilter`.
  """
  def __init__(self):
    super(InfraFormatter, self).__init__(
        '[%(severity)s%(iso8601)s %(process)d %(thread)d '
        '%(fullModuleName)s:%(lineno)s] %(message)s')


def add_handler(logger, handler=None, timezone='UTC',
                level=logging.WARNING,
                module_name_blacklist=None):  # pragma: no cover
  """Configures and adds a handler to a logger the standard way for infra.

  Args:
    logger (logging.Logger): logger object obtained from `logging.getLogger`.

  Keyword Args:
    handler (logging.Handler): handler to add to the logger. defaults to
       logging.StreamHandler.
    timezone (str): timezone to use for timestamps.
    level (int): logging level. Could be one of DEBUG, INFO, WARNING, CRITICAL
    module_name_blacklist (str): do not print log lines from modules whose name
      matches this regular expression.

  Example usage::

    import logging
    import infra_libs.logs
    logger = logging.getLogger('foo')
    infra_libs.logs.add_handler(logger, timezone='US/Pacific')
    logger.info('test message')

  The last line should print something like::

    [I2014-06-27T11:42:32.418716-07:00 7082 logs:71] test message

  """
  handler = handler or logging.StreamHandler()
  handler.addFilter(InfraFilter(timezone,
                                module_name_blacklist=module_name_blacklist))
  handler.setFormatter(InfraFormatter())
  handler.setLevel(level=level)
  logger.addHandler(handler)

  # Formatters only get messages that pass this filter: let everything through.
  logger.setLevel(level=logging.DEBUG)


def default_program_name():
  # Use argv[0] as the program name, except when it's '__main__.py' which is the
  # case when we were invoked by run.py.  In this case look at the main module's
  # __package__ variable which is set by runpy.
  ret = os.path.basename(sys.argv[0])
  if ret == '__main__.py':
    package = sys.modules['__main__'].__package__
    if package is not None:
      return package.split('.')[-1]
  return ret


def add_argparse_options(parser,
                         default_level=logging.WARNING):  # pragma: no cover
  """Adds logging related options to an argparse.ArgumentParser.

  See also: :func:`process_argparse_options`
  """

  parser = parser.add_argument_group('Logging Options')
  g = parser.add_mutually_exclusive_group()
  g.set_defaults(log_level=default_level)
  g.add_argument('--logs-quiet', '--quiet',
                 action='store_const', const=logging.ERROR,
                 dest='log_level', help='Make the output quieter (ERROR).')
  g.add_argument('--logs-warning', '--warning',
                 action='store_const', const=logging.WARNING,
                 dest='log_level',
                 help='Set the output to an average verbosity (WARNING).')
  g.add_argument('--logs-verbose', '--verbose',
                 action='store_const', const=logging.INFO,
                 dest='log_level', help='Make the output louder (INFO).')
  g.add_argument('--logs-debug', '--debug',
                 action='store_const', const=logging.DEBUG,
                 dest='log_level', help='Make the output really loud (DEBUG).')
  parser.add_argument('--logs-black-list', metavar='REGEX',
                      help='hide log lines emitted by modules whose name '
                           'matches\nthis regular expression.')
  parser.add_argument(
      '--logs-directory',
      default=DEFAULT_LOG_DIRECTORIES,
      help=textwrap.fill(
          'directory into which to write logs (default: %%(default)s). If '
          'this directory does not exist or is not writable, the temporary '
          'directory (%s) will be used instead. If this is explicitly set '
          'to the empty string, logs will not be written at all. May be set '
          'to multiple directories separated by the "%s" character, in '
          'which case the first one that exists and is writable is used.' % (
                tempfile.gettempdir(), os.pathsep), width=56))
  parser.add_argument(
      '--logs-program-name',
      default=default_program_name(),
      help='the program name used to name the log files created in '
           '--logs-directory (default: %(default)s).')
  parser.add_argument(
      '--logs-debug-file',
      action='store_true',
      help='by default only INFO, WARNING and ERROR log files are written to '
           'disk.  This flag causes a DEBUG log to be written as well.')


def process_argparse_options(options, logger=None):  # pragma: no cover
  """Handles logging argparse options added in 'add_argparse_options'.

  Configures 'logging' module.

  Args:
    options: return value of argparse.ArgumentParser.parse_args.
    logger (logging.Logger): logger to apply the configuration to.

  Example usage::

    import argparse
    import sys
    import infra_libs.logs

    parser = argparse.ArgumentParser()
    infra_libs.logs.add_argparse_options(parser)

    options = parser.parse_args(sys.path[1:])
    infra_libs.logs.process_argparse_options(options)
  """

  if logger is None:
    logger = logging.root

  add_handler(logger, level=options.log_level,
              module_name_blacklist=options.logs_black_list)

  if options.logs_directory:
    _add_file_handlers(options, logger)


def _add_file_handlers(options, logger):  # pragma: no cover
  # Test whether we can write to the log directory.  If not, write to a
  # temporary directory instead.  One of the DEFAULT_LOG_DIRECTORIES are created
  # on the real production machines by puppet, so /tmp should only be used when
  # running locally on developers' workstations.
  logs_directory = tempfile.gettempdir()
  for directory in options.logs_directory.split(os.pathsep):
    if not os.path.isdir(directory):
      continue

    try:
      with tempfile.TemporaryFile(dir=directory):
        pass
    except OSError:
      pass
    else:
      logs_directory = directory
      break

  # Log files are named with this pattern:
  # <program>.<hostname>.<username>.log.<level>.YYYYMMDD-HHMMSS.<pid>
  pattern = "%s.%s.%s.log.%%s.%s.%d" % (
      options.logs_program_name,
      socket.getfqdn().split('.')[0],
      getpass.getuser(),
      datetime.datetime.utcnow().strftime('%Y%m%d-%H%M%S'),
      os.getpid())

  file_levels = [logging.INFO, logging.WARNING, logging.ERROR]
  if options.logs_debug_file:
    file_levels.append(logging.DEBUG)

  for level in file_levels:
    add_handler(
        logger,
        handler=logging.handlers.RotatingFileHandler(
            filename=os.path.join(
                logs_directory, pattern % logging.getLevelName(level)),
            maxBytes=10 * 1024 * 1024,
            backupCount=10,
            delay=True),
        level=level)
