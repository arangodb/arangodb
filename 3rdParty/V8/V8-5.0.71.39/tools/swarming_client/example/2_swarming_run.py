#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolate, remotely on a Swarming
bot.

It first 'compiles' hello_world.isolate into hello_word.isolated, then requests
via swarming.py to archives, run and collect results for this task.

It generates example_result.json as a task summary.
"""

import os
import shutil
import subprocess
import sys
import tempfile

# Pylint can't find common.py that's in the same directory as this file.
# pylint: disable=F0401
import common


def main():
  options = common.parse_args(use_isolate_server=True, use_swarming=True)
  tempdir = tempfile.mkdtemp(prefix=u'hello_world')
  try:
    isolated, _ = common.isolate(
        tempdir, options.isolate_server, options.swarming_os, options.verbose)
    common.note(
        'Running the job remotely. This:\n'
        ' - archives to %s\n'
        ' - runs and collect results via %s' %
        (options.isolate_server, options.swarming))
    cmd = [
      'swarming.py',
      'run',
      '--swarming', options.swarming,
      '--isolate-server', options.isolate_server,
      '--dimension', 'os', options.swarming_os,
      '--task-name', options.task_name,
      '--task-summary-json', 'example_result.json',
      '--decorate',
      isolated,
    ]
    if options.idempotent:
      cmd.append('--idempotent')
    if options.priority is not None:
      cmd.extend(('--priority', str(options.priority)))
    common.run(cmd, options.verbose)
    with open('example_result.json', 'rb') as f:
      print('example_result.json content:')
      print(f.read())
    return 0
  except subprocess.CalledProcessError as e:
    return e.returncode
  finally:
    shutil.rmtree(tempdir)


if __name__ == '__main__':
  sys.exit(main())
