#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolated, locally in a temporary
directory.

The files are archived and fetched from the remote Isolate Server.
"""

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

# Pylint can't find common.py that's in the same directory as this file.
# pylint: disable=F0401
import common


def main():
  options = common.parse_args(use_isolate_server=True, use_swarming=False)
  tempdir = tempfile.mkdtemp(prefix=u'hello_world')
  try:
    # All the files are put in a temporary directory. This is optional and
    # simply done so the current directory doesn't have the following files
    # created:
    # - hello_world.isolated
    # - hello_world.isolated.state
    # - cache/
    cachedir = os.path.join(tempdir, 'cache')
    isolateddir = os.path.join(tempdir, 'isolated')
    isolated = os.path.join(isolateddir, 'hello_world.isolated')

    os.mkdir(isolateddir)

    common.note('Archiving to %s' % options.isolate_server)
    # TODO(maruel): Parse the output from run() to get 'isolated_sha1'.
    common.run(
        [
          'isolate.py',
          'archive',
          '--isolate', os.path.join('payload', 'hello_world.isolate'),
          '--isolated', isolated,
          '--isolate-server', options.isolate_server,
          '--config-variable', 'OS', 'Yours',
        ], options.verbose)

    common.note(
        'Downloading from %s and running in a temporary directory' %
        options.isolate_server)
    with open(isolated, 'rb') as f:
      isolated_sha1 = hashlib.sha1(f.read()).hexdigest()
    common.run(
        [
          'run_isolated.py',
          '--cache', cachedir,
          '--isolate-server', options.isolate_server,
          '--isolated', isolated_sha1,
          '--no-log',
        ], options.verbose)
    return 0
  except subprocess.CalledProcessError as e:
    return e.returncode
  finally:
    shutil.rmtree(tempdir)


if __name__ == '__main__':
  sys.exit(main())
