# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Toolset to run multiple Swarming tasks in parallel."""

import getpass
import json
import os
import subprocess
import sys
import tempfile
import time

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_DIR)

sys.path.insert(0, ROOT_DIR)

import auth
import isolateserver
from utils import logging_utils
from utils import threading_utils
from utils import tools


def task_to_name(name, dimensions, isolated_hash):
  """Returns a task name the same way swarming.py generates them."""
  return '%s/%s/%s' % (
      name,
      '_'.join('%s=%s' % (k, v) for k, v in sorted(dimensions.iteritems())),
      isolated_hash)


def capture(cmd):
  assert all(isinstance(i, basestring) for i in cmd), cmd
  start = time.time()
  p = subprocess.Popen(
      [sys.executable] + cmd, cwd=ROOT_DIR, stdout=subprocess.PIPE)
  out = p.communicate()[0]
  return p.returncode, out, time.time() - start


def trigger(swarming_server, isolate_server, task_name, isolated_hash, args):
  """Triggers a specified .isolated file."""
  fd, jsonfile = tempfile.mkstemp(prefix=u'swarming')
  os.close(fd)
  try:
    cmd = [
      'swarming.py', 'trigger',
      '--swarming', swarming_server,
      '--isolate-server', isolate_server,
      '--task-name', task_name,
      '--dump-json', jsonfile,
      isolated_hash,
    ]
    returncode, out, duration = capture(cmd + args)
    with open(jsonfile) as f:
      data = json.load(f)
    task_id = str(data['tasks'][task_name]['task_id'])
    return returncode, out, duration, task_id
  finally:
    os.remove(jsonfile)


def collect(swarming_server, task_id):
  """Collects results of a swarming task."""
  cmd = ['swarming.py', 'collect', '--swarming', swarming_server, task_id]
  return capture(cmd)


class Runner(object):
  """Runners runs tasks in parallel on Swarming."""
  def __init__(
      self, swarming_server, isolate_server, add_task, progress,
      extra_trigger_args):
    self.swarming_server = swarming_server
    self.isolate_server = isolate_server
    self.add_task = add_task
    self.progress = progress
    self.extra_trigger_args = extra_trigger_args

  def trigger(self, task_name, isolated_hash, dimensions):
    args = sum((['--dimension', k, v] for k, v in dimensions.iteritems()), [])
    returncode, stdout, duration, task_id = trigger(
        self.swarming_server,
        self.isolate_server,
        task_name,
        isolated_hash,
        self.extra_trigger_args + args)
    step_name = '%s (%3.2fs)' % (task_name, duration)
    if returncode:
      line = 'Failed to trigger %s\n%s' % (step_name, stdout)
      self.progress.update_item(line, index=1)
      return
    self.progress.update_item('Triggered %s' % step_name, index=1)
    self.add_task(0, self.collect, task_name, task_id, dimensions)

  def collect(self, task_name, task_id, dimensions):
    returncode, stdout, duration = collect(self.swarming_server, task_id)
    step_name = '%s (%3.2fs)' % (task_name, duration)
    if returncode:
      # Only print the output for failures, successes are unexciting.
      self.progress.update_item(
          'Failed %s:\n%s' % (step_name, stdout), index=1)
      return (task_name, dimensions, stdout)
    self.progress.update_item('Passed %s' % step_name, index=1)


def run_swarming_tasks_parallel(
    swarming_server, isolate_server, extra_trigger_args, tasks):
  """Triggers swarming tasks in parallel and gets results.

  This is done by using one thread per task and shelling out swarming.py.

  Arguments:
    extra_trigger_args: list of additional flags to pass down to
        'swarming.py trigger'
    tasks: list of tuple(task_name, isolated_hash, dimensions) where dimension
        are --dimension flags to provide when triggering the task.

  Yields:
    tuple(name, dimensions, stdout) for the tasks that failed.
  """
  runs = len(tasks)
  # triger + collect
  total = 2 * runs
  failed_tasks = []
  progress = threading_utils.Progress([('index', 0), ('size', total)])
  progress.use_cr_only = False
  start = time.time()
  with threading_utils.ThreadPoolWithProgress(
      progress, runs, runs, total) as pool:
    runner = Runner(
        swarming_server, isolate_server, pool.add_task, progress,
        extra_trigger_args)

    for task_name, isolated_hash, dimensions in tasks:
      pool.add_task(0, runner.trigger, task_name, isolated_hash, dimensions)

    # Runner.collect() only return task failures.
    for failed_task in pool.iter_results():
      task_name, dimensions, stdout = failed_task
      yield task_name, dimensions, stdout
      failed_tasks.append(task_name)

  duration = time.time() - start
  print('\nCompleted in %3.2fs' % duration)
  if failed_tasks:
    print('Detected the following failures:')
    for task in sorted(failed_tasks):
      print('  %s' % task)


class OptionParser(logging_utils.OptionParserWithLogging):
  def __init__(self, **kwargs):
    logging_utils.OptionParserWithLogging.__init__(self, **kwargs)
    self.server_group = tools.optparse.OptionGroup(self, 'Server')
    self.server_group.add_option(
        '-S', '--swarming',
        metavar='URL', default=os.environ.get('SWARMING_SERVER', ''),
        help='Swarming server to use')
    isolateserver.add_isolate_server_options(self.server_group)
    self.add_option_group(self.server_group)
    auth.add_auth_options(self)
    self.add_option(
        '-d', '--dimension', default=[], action='append', nargs=2,
        dest='dimensions', metavar='FOO bar',
        help='dimension to filter on')
    self.add_option(
        '--priority', type='int',
        help='The lower value, the more important the task is. It may be '
            'important to specify a higher priority since the default value '
            'will make the task to be triggered only when the bots are idle.')
    self.add_option(
        '--deadline', type='int', default=6*60*60,
        help='Seconds to allow the task to be pending for a bot to run before '
            'this task request expires.')

  def parse_args(self, *args, **kwargs):
    options, args = logging_utils.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    options.swarming = options.swarming.rstrip('/')
    if not options.swarming:
      self.error('--swarming is required.')
    auth.process_auth_options(self, options)
    isolateserver.process_isolate_server_options(self, options, False)
    options.dimensions = dict(options.dimensions)
    return options, args

  def format_description(self, _):
    return self.description
