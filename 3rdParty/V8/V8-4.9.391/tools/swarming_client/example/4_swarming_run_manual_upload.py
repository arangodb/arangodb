#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs hello_world.py through a manually crafted hello_world.isolated, remotely
on a Swarming slave.

No .isolate file is involved at all.

It creates hello_world.isolated and archives via 'isolateserver.py archive',
then trigger and finally collect the results.

It never create a local file.
"""

import json
import os
import subprocess
import sys
import tempfile

# Pylint can't find common.py that's in the same directory as this file.
# pylint: disable=F0401
import common


def main():
  options = common.parse_args(use_isolate_server=True, use_swarming=True)
  try:
    common.note(
        'Archiving directory \'payload\' to %s' % options.isolate_server)
    payload_isolated_sha1 = common.capture(
        [
          'isolateserver.py',
          'archive',
          '--isolate-server', options.isolate_server,
          'payload',
        ]).split()[0]

    common.note(
        'Archiving custom .isolated file to %s' % options.isolate_server)
    handle, isolated = tempfile.mkstemp(
        prefix=u'hello_world', suffix=u'.isolated')
    os.close(handle)
    try:
      data = {
        'algo': 'sha-1',
        'command': ['python', 'hello_world.py', 'Custom'],
        'includes': [payload_isolated_sha1],
        'version': '1.0',
      }
      with open(isolated, 'wb') as f:
        json.dump(data, f, sort_keys=True, separators=(',',':'))
      isolated_sha1 = common.capture(
          [
            'isolateserver.py',
            'archive',
            '--isolate-server', options.isolate_server,
            isolated,
          ]).split()[0]
    finally:
      common.note('Deleting temporary file, it is not necessary anymore.')
      os.remove(isolated)

    # Now trigger as usual. You could look at run_exmaple_swarming_involved for
    # the involved way but use the short way here.

    common.note('Running %s on %s' % (isolated_sha1, options.swarming))
    cmd = [
      'swarming.py',
      'run',
      '--swarming', options.swarming,
      '--isolate-server', options.isolate_server,
      '--dimension', 'os', options.swarming_os,
      '--task-name', options.task_name,
      isolated_sha1,
    ]
    if options.idempotent:
      cmd.append('--idempotent')
    if options.priority is not None:
      cmd.extend(('--priority', str(options.priority)))
    common.run(cmd, options.verbose)
    return 0
  except subprocess.CalledProcessError as e:
    return e.returncode


if __name__ == '__main__':
  sys.exit(main())
