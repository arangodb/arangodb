#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolate, remotely on a Swarming
slave.

It compiles and archives via 'isolate.py archive', then discard the local files.
After, it triggers and finally collects the results.

Creates 2 shards and instructs the script to produce a file in the output
directory.
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
  try:
    tempdir = tempfile.mkdtemp(prefix=u'hello_world')
    try:
      _, hashval = common.isolate(
          tempdir, options.isolate_server, options.swarming_os, options.verbose)

      json_file = os.path.join(tempdir, 'task.json')
      common.note('Running on %s' % options.swarming)
      cmd = [
        'swarming.py',
        'trigger',
        '--swarming', options.swarming,
        '--isolate-server', options.isolate_server,
        '--dimension', 'os', options.swarming_os,
        '--task-name', options.task_name,
        '--dump-json', json_file,
        '--isolated', hashval,
        '--shards', '2',
      ]
      if options.idempotent:
        cmd.append('--idempotent')
      if options.priority is not None:
        cmd.extend(('--priority', str(options.priority)))
      cmd.extend(('--', '${ISOLATED_OUTDIR}'))
      common.run(cmd, options.verbose)

      common.note('Getting results from %s' % options.swarming)
      common.run(
          [
            'swarming.py',
            'collect',
            '--swarming', options.swarming,
            '--json', json_file,
            '--task-output-dir', 'example_result',
          ], options.verbose)
      for root, _, files in os.walk('example_result'):
        for name in files:
          p = os.path.join(root, name)
          with open(p, 'rb') as f:
            print('%s content:' % p)
            print(f.read())
      return 0
    finally:
      shutil.rmtree(tempdir)
  except subprocess.CalledProcessError as e:
    return e.returncode


if __name__ == '__main__':
  sys.exit(main())
