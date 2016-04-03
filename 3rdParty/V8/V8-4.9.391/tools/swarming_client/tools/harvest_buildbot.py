#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Harvest data on the Try Server.

Please use sparingly. Large values for horizon will trash the Try Server memory.
"""

import os
import optparse
import sys


def get_failed_builds(builder, horizon):
  """Constructs the list of failed builds."""
  builder.builds.cache()
  return [
      builder.builds[i] for i in xrange(-horizon, 0)
      if not builder.builds[i].simplified_result
  ]


def get_parent_build(build):
  """Returns the parent build for a triggered build."""
  parent_buildername = build.properties_as_dict['parent_buildername']
  parent_builder = build.builders[parent_buildername]
  return parent_builder.builds[build.properties_as_dict['parent_buildnumber']]


def parse(b, horizon):
  print('Processing last %d entries' % horizon)
  failed_builds = get_failed_builds(b.builders['swarm_triggered'], horizon)
  rate = 100. * (1. - float(len(failed_builds)) / float(horizon))
  print('Success: %3.1f%%' % rate)

  NO_KEY = 'Warning: Unable to find any tests with the name'
  HTTP_404 = 'threw HTTP Error 404: Not Found'
  for build in failed_builds:
    print('%s/%d on %s' % (build.builder.name, build.number, build.slave.name))
    swarm_trigger_tests = build.steps['swarm_trigger_tests']
    base_unittests = build.steps['base_unittests']
    fail = ''
    if swarm_trigger_tests.simplified_result:
      fail = 'buildbot failed to pass arguments'
    elif not swarm_trigger_tests.stdio:
      fail = 'old swarm_trigger_step.py version'
    else:
      stdio = base_unittests.stdio
      if NO_KEY in stdio:
        fail = 'Failed to retrieve keys.'
      elif HTTP_404 in stdio:
        fail = 'Failed to retrieve keys with 404.'

    if fail:
      print('  Triggering failed: %s' % fail)
    else:
      # Print the first few lines.
      lines = base_unittests.stdio.splitlines()[:15]
      print('\n'.join('  ' + l for l in lines))
    print('  %s  %s  %s  %s' % (
      build.properties_as_dict['use_swarm_client_revision'],
      build.properties_as_dict['swarm_hashes'],
      build.properties_as_dict.get('use_swarm_client_revision'),
      build.properties_as_dict.get('testfilter')))


def main():
  parser = optparse.OptionParser()
  parser.add_option('-b', '--buildbot_json', help='path to buildbot_json.py')
  parser.add_option(
      '-u', '--url',
      default='http://build.chromium.org/p/tryserver.chromium/',
      help='server url, default: %default')
  parser.add_option('-H', '--horizon', default=100, type='int')
  options, args = parser.parse_args(None)
  if args:
    parser.error('Unsupported args: %s' % args)

  if options.horizon < 10 or options.horizon > 2000:
    parser.error('Use reasonable --horizon value')

  if options.buildbot_json:
    options.buildbot_json = os.path.abspath(options.buildbot_json)
    if not os.path.isdir(options.buildbot_json):
      parser.error('Pass a valid directory path to --buildbot_json')
    sys.path.insert(0, options.buildbot_json)

  try:
    import buildbot_json  # pylint: disable=F0401
  except ImportError:
    parser.error('Pass a directory path to buildbot_json.py with -b')

  b = buildbot_json.Buildbot(options.url)
  parse(b, options.horizon)
  return 0


if __name__ == '__main__':
  sys.exit(main())
