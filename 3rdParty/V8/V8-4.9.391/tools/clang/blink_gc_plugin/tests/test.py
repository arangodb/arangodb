#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
tool_dir = os.path.abspath(os.path.join(script_dir, '../../pylib'))
sys.path.insert(0, tool_dir)

from clang import plugin_testing


class BlinkGcPluginTest(plugin_testing.ClangPluginTest):
  """Test harness for the Blink GC plugin."""

  def AdjustClangArguments(self, clang_cmd):
    clang_cmd.append('-Wno-inaccessible-base')

  def ProcessOneResult(self, test_name, actual):
    # Some Blink GC plugins dump a JSON representation of the object graph, and
    # use the processed results as the actual results of the test.
    if os.path.exists('%s.graph.json' % test_name):
      try:
        actual = subprocess.check_output(
            ['python', '../process-graph.py', '-c',
             '%s.graph.json' % test_name],
            stderr=subprocess.STDOUT)
      except subprocess.CalledProcessError, e:
        # The graph processing script returns a failure exit code if the graph
        # is bad (e.g. it has a cycle). The output still needs to be captured in
        # that case, since the expected results capture the errors.
        actual = e.output
      finally:
        # Clean up the .graph.json file to prevent false passes from stale
        # results from a previous run.
        os.remove('%s.graph.json' % test_name)
    return super(BlinkGcPluginTest, self).ProcessOneResult(test_name, actual)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--reset-results',
      action='store_true',
      help='If specified, overwrites the expected results in place.')
  parser.add_argument('clang_path', help='The path to the clang binary.')
  parser.add_argument('plugin_path',
                      nargs='?',
                      help='The path to the plugin library, if any.')
  args = parser.parse_args()

  return BlinkGcPluginTest(
      os.path.dirname(os.path.realpath(__file__)),
      args.clang_path,
      args.plugin_path,
      'blink-gc-plugin',
      args.reset_results).Run()


if __name__ == '__main__':
  sys.exit(main())
