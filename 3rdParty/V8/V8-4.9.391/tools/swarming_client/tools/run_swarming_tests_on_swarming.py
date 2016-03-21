#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs the whole set of swarming client unit tests on swarming itself.

This is done in a few steps:
  - Archive the whole directory as a single .isolated file.
  - Create one test-specific .isolated for each test to run. The file is created
    directly and archived manually with isolateserver.py.
  - Trigger each of these test-specific .isolated file per OS.
  - Get all results out of order.
"""

__version__ = '0.1'

import glob
import logging
import os
import subprocess
import sys
import tempfile
import time

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Must be first import.
import parallel_execution

from third_party import colorama
from third_party.depot_tools import fix_encoding
from utils import file_path
from utils import tools


def check_output(cmd):
  return subprocess.check_output([sys.executable] + cmd, cwd=ROOT_DIR)


def archive_tree(isolate_server):
  """Archives a whole tree and return the sha1 of the .isolated file.

  Manually creates a temporary isolated file and archives it.
  """
  cmd = [
      'isolateserver.py', 'archive', '--isolate-server', isolate_server,
      ROOT_DIR, '--blacklist="\\.git"',
  ]
  if logging.getLogger().isEnabledFor(logging.INFO):
    cmd.append('--verbose')
  out = check_output(cmd)
  return out.split()[0]


def archive_isolated_triggers(isolate_server, tree_isolated, tests):
  """Creates and archives all the .isolated files for the tests at once.

  Archiving them in one batch is faster than archiving each file individually.
  Also the .isolated files can be reused across OSes, reducing the amount of
  I/O.

  Returns:
    list of (test, sha1) tuples.
  """
  logging.info('archive_isolated_triggers(%s, %s)', tree_isolated, tests)
  tempdir = tempfile.mkdtemp(prefix=u'run_swarming_tests_on_swarming_')
  try:
    isolateds = []
    for test in tests:
      test_name = os.path.basename(test)
      # Creates a manual .isolated file. See
      # https://code.google.com/p/swarming/wiki/IsolatedDesign for more details.
      isolated = {
        'algo': 'sha-1',
        'command': ['python', test],
        'includes': [tree_isolated],
        'read_only': 0,
        'version': '1.4',
      }
      v = os.path.join(tempdir, test_name + '.isolated')
      tools.write_json(v, isolated, True)
      isolateds.append(v)
    cmd = [
        'isolateserver.py', 'archive', '--isolate-server', isolate_server,
    ] + isolateds
    if logging.getLogger().isEnabledFor(logging.INFO):
      cmd.append('--verbose')
    items = [i.split() for i in check_output(cmd).splitlines()]
    assert len(items) == len(tests)
    assert all(
        items[i][1].endswith(os.path.basename(tests[i]) + '.isolated')
        for i in xrange(len(tests)))
    return zip(tests, [i[0] for i in items])
  finally:
    file_path.rmtree(tempdir)



def run_swarming_tests_on_swarming(
    swarming_server, isolate_server, priority, oses, tests, logs,
    no_idempotent):
  """Archives, triggers swarming jobs and gets results."""
  print('Archiving the whole tree.')
  start = time.time()
  tree_isolated = archive_tree(isolate_server)

  # Create and archive all the .isolated files.
  isolateds = archive_isolated_triggers(isolate_server, tree_isolated, tests)
  print('Archival took %3.2fs' % (time.time() - start))

  exploded = []
  for test_path, isolated_hash in isolateds:
    logging.debug('%s: %s', test_path, isolated_hash)
    test_name = os.path.basename(test_path).split('.')[0]
    for platform in oses:
      exploded.append((test_name, platform, isolated_hash))

  tasks = [
    (
      parallel_execution.task_to_name(name, {'os': platform}, isolated_hash),
      isolated_hash,
      {'os': platform},
    ) for name, platform, isolated_hash in exploded
  ]

  extra_args = [
    '--hard-timeout', '180',
  ]
  if not no_idempotent:
    extra_args.append('--idempotent')
  if priority:
    extra_args.extend(['--priority', str(priority)])
    print('Using priority %s' % priority)

  result = 0
  for failed_task in parallel_execution.run_swarming_tasks_parallel(
      swarming_server, isolate_server, extra_args, tasks):
    test_name, dimensions, stdout = failed_task
    if logs:
      # Write the logs are they are retrieved.
      if not os.path.isdir(logs):
        os.makedirs(logs)
      name = '%s_%s.log' % (dimensions['os'], test_name.split('/', 1)[0])
      with open(os.path.join(logs, name), 'wb') as f:
        f.write(stdout)
    result = 1
  return result


def main():
  parser = parallel_execution.OptionParser(
              usage='%prog [options]', version=__version__)
  parser.add_option(
      '--logs',
      help='Destination where to store the failure logs (recommended!)')
  parser.add_option('-o', '--os', help='Run tests only on this OS')
  parser.add_option(
      '-t', '--test', action='append',
      help='Run only these test, can be specified multiple times')
  parser.add_option(
      '--no-idempotent', action='store_true',
      help='Do not use --idempotent to detect flaky tests')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported argument %s' % args)

  oses = ['Linux', 'Mac', 'Windows']
  tests = [
      os.path.relpath(i, ROOT_DIR)
      for i in (
      glob.glob(os.path.join(ROOT_DIR, 'tests', '*_test.py')) +
      glob.glob(os.path.join(ROOT_DIR, 'googletest', 'tests', '*_test.py')))
  ]
  valid_tests = sorted(map(os.path.basename, tests))
  assert len(valid_tests) == len(set(valid_tests)), (
      'Can\'t have 2 tests with the same base name')

  if options.test:
    for t in options.test:
      if not t in valid_tests:
        parser.error(
            '--test %s is unknown. Valid values are:\n%s' % (
              t, '\n'.join('  ' + i for i in valid_tests)))
    filters = tuple(os.path.sep + t for t in options.test)
    tests = [t for t in tests if t.endswith(filters)]

  if options.os:
    if options.os not in oses:
      parser.error(
          '--os %s is unknown. Valid values are %s' % (
            options.os, ', '.join(sorted(oses))))
    oses = [options.os]

  if sys.platform in ('win32', 'cygwin'):
    # If we are on Windows, don't generate the tests for Linux and Mac since
    # they use symlinks and we can't create symlinks on windows.
    oses = ['Windows']
    if options.os != 'win32':
      print('Linux and Mac tests skipped since running on Windows.')

  return run_swarming_tests_on_swarming(
      options.swarming,
      options.isolate_server,
      options.priority,
      oses,
      tests,
      options.logs,
      options.no_idempotent)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main())
