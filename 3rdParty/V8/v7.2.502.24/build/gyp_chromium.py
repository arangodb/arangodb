# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is now only used by the closure_compilation builders."""

import gyp_environment
import os
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
chrome_src = os.path.abspath(os.path.join(script_dir, os.pardir))

sys.path.insert(0, os.path.join(chrome_src, 'tools', 'gyp', 'pylib'))
import gyp


def main():
  gyp_environment.SetEnvironment()

  print 'Updating projects from gyp files...'
  sys.stdout.flush()
  sys.exit(gyp.main(sys.argv[1:] + [
      '--check',
      '--no-circular-check',
      '-I', os.path.join(script_dir, 'common.gypi'),
      '-D', 'gyp_output_dir=out']))

if __name__ == '__main__':
  sys.exit(main())
