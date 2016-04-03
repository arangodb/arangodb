#!/usr/bin/env python
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Automated maintenance tool to run a script on bots.

To use this script, write a self-contained python script (use a .zip if
necessary), specify it on the command line and it will be packaged and triggered
on all the swarming bots corresponding to the --dimension filters specified, or
all the bots if no filter is specified.
"""

__version__ = '0.1'

import os
import tempfile
import shutil
import subprocess
import sys

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Must be first import.
import parallel_execution

from third_party import colorama
from third_party.depot_tools import fix_encoding
from utils import file_path
from utils import tools


def get_bot_list(swarming_server, dimensions, dead_only):
  """Returns a list of swarming bots."""
  cmd = [
    sys.executable, 'swarming.py', 'bots',
    '--swarming', swarming_server,
    '--bare',
  ]
  for k, v in sorted(dimensions.iteritems()):
    cmd.extend(('--dimension', k, v))
  if dead_only:
    cmd.append('--dead-only')
  return subprocess.check_output(cmd, cwd=ROOT_DIR).splitlines()


def archive(isolate_server, script):
  """Archives the tool and return the sha-1."""
  base_script = os.path.basename(script)
  isolate = {
    'variables': {
      'command': ['python', base_script],
      'files': [base_script],
    },
  }
  tempdir = tempfile.mkdtemp(prefix=u'run_on_bots')
  try:
    isolate_file = os.path.join(tempdir, 'tool.isolate')
    isolated_file = os.path.join(tempdir, 'tool.isolated')
    with open(isolate_file, 'wb') as f:
      f.write(str(isolate))
    shutil.copyfile(script, os.path.join(tempdir, base_script))
    cmd = [
      sys.executable, 'isolate.py', 'archive',
      '--isolate-server', isolate_server,
      '-i', isolate_file,
      '-s', isolated_file,
    ]
    return subprocess.check_output(cmd, cwd=ROOT_DIR).split()[0]
  finally:
    file_path.rmtree(tempdir)


def run_serial(
    swarming_server, isolate_server, priority, deadline, repeat, isolated_hash,
    name, bots):
  """Runs the task one at a time.

  This will be mainly bound by task scheduling latency, especially if the bots
  are busy and the priority is low.
  """
  result = 0
  for i in xrange(repeat):
    for bot in bots:
      suffix = '/%d' % i if repeat > 1 else ''
      task_name = parallel_execution.task_to_name(
          name, {'id': bot}, isolated_hash) + suffix
      cmd = [
        sys.executable, 'swarming.py', 'run',
        '--swarming', swarming_server,
        '--isolate-server', isolate_server,
        '--priority', priority,
        '--deadline', deadline,
        '--dimension', 'id', bot,
        '--task-name', task_name,
        isolated_hash,
      ]
      r = subprocess.call(cmd, cwd=ROOT_DIR)
      result = max(r, result)
  return result


def run_parallel(
    swarming_server, isolate_server, priority, deadline, repeat, isolated_hash,
    name, bots):
  tasks = []
  for i in xrange(repeat):
    suffix = '/%d' % i if repeat > 1 else ''
    tasks.extend(
        (
          parallel_execution.task_to_name(
              name, {'id': bot}, isolated_hash) + suffix,
          isolated_hash,
          {'id': bot},
        ) for bot in bots)
  extra_args = ['--priority', priority, '--deadline', deadline]
  print('Using priority %s' % priority)
  for failed_task in parallel_execution.run_swarming_tasks_parallel(
      swarming_server, isolate_server, extra_args, tasks):
    _name, dimensions, stdout = failed_task
    print('%sFailure: %s%s\n%s' % (
      colorama.Fore.RED, dimensions, colorama.Fore.RESET, stdout))


def main():
  parser = parallel_execution.OptionParser(
      usage='%prog [options] script.py', version=__version__)
  parser.add_option(
      '--serial', action='store_true',
      help='Runs the task serially, to be used when debugging problems since '
           'it\'s slow')
  parser.add_option(
      '--repeat', type='int', default=1,
      help='Runs the task multiple time on each bot, meant to be used as a '
           'load test')
  options, args = parser.parse_args()

  if len(args) != 1:
    parser.error(
        'Must pass one python script to run. Use --help for more details')

  if not options.priority:
    parser.error(
        'Please provide the --priority option. Either use a very low number\n'
        'so the task completes as fast as possible, or an high number so the\n'
        'task only runs when the bot is idle.')

  # 1. Query the bots list.
  bots = get_bot_list(options.swarming, options.dimensions, False)
  print('Found %d bots to process' % len(bots))
  if not bots:
    return 1

  dead_bots = get_bot_list(options.swarming, options.dimensions, True)
  if dead_bots:
    print('Warning: found %d dead bots' % len(dead_bots))

  # 2. Archive the script to run.
  isolated_hash = archive(options.isolate_server, args[0])
  print('Running %s' % isolated_hash)

  # 3. Trigger the tasks.
  name = os.path.basename(args[0])
  if options.serial:
    return run_serial(
        options.swarming,
        options.isolate_server,
        str(options.priority),
        str(options.deadline),
        options.repeat,
        isolated_hash,
        name,
        bots)

  return run_parallel(
      options.swarming,
      options.isolate_server,
      str(options.priority),
      str(options.deadline),
      options.repeat,
      isolated_hash,
      name,
      bots)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main())
