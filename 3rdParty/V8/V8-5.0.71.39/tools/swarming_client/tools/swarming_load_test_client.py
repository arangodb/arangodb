#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Triggers a ton of fake jobs to test its handling under high load.

Generates an histogram with the latencies to process the tasks and number of
retries.
"""

import json
import logging
import optparse
import os
import random
import re
import string
import sys
import time

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sys.path.insert(0, ROOT_DIR)

from third_party import colorama

import swarming

from utils import graph
from utils import net
from utils import threading_utils

import swarming_load_test_bot


# Amount of time the timer should be reduced on the Swarming side.
TIMEOUT_OVERHEAD = 10


def print_results(results, columns, buckets):
  delays = [i for i in results if isinstance(i, float)]
  failures = [i for i in results if not isinstance(i, float)]

  graph.print_histogram(
      graph.generate_histogram(delays, buckets), columns, '%5.3f')
  print('')
  print('Total items : %d' % len(results))
  average = 0
  if delays:
    average = sum(delays)/ len(delays)
  print('Average delay: %.2fs' % average)
  #print('Average overhead: %s' % graph.to_units(total_size / len(sizes)))
  print('')
  if failures:
    print('')
    print('%sFAILURES%s:' % (colorama.Fore.RED, colorama.Fore.RESET))
    print('\n'.join('  %s' % i for i in failures))


def trigger_task(
    swarming_url, dimensions, sleep_time, output_size, progress,
    unique, timeout, index):
  """Triggers a Swarming job and collects results.

  Returns the total amount of time to run a task remotely, including all the
  overhead.
  """
  name = 'load-test-%d-%s' % (index, unique)
  start = time.time()

  logging.info('trigger')
  manifest = swarming.Manifest(
    isolate_server='http://localhost:1',
    namespace='dummy-isolate',
    isolated_hash=1,
    task_name=name,
    extra_args=[],
    env={},
    dimensions=dimensions,
    deadline=int(timeout-TIMEOUT_OVERHEAD),
    verbose=False,
    profile=False,
    priority=100)
  cmd = [
    'python',
    '-c',
    'import time; print(\'1\'*%s); time.sleep(%d); print(\'Back\')' %
    (output_size, sleep_time)
  ]
  manifest.add_task('echo stuff', cmd)
  data = {'request': manifest.to_json()}
  response = net.url_open(swarming_url + '/test', data=data)
  if not response:
    # Failed to trigger. Return a failure.
    return 'failed_trigger'

  result = json.load(response)
  # Old API uses harcoded config name. New API doesn't have concept of config
  # name so it uses the task name. Ignore this detail.
  test_keys = []
  for key in result['test_keys']:
    key.pop('config_name')
    test_keys.append(key.pop('test_key'))
    assert re.match('[0-9a-f]+', test_keys[-1]), test_keys
  expected = {
    u'priority': 100,
    u'test_case_name': unicode(name),
    u'test_keys': [
      {
        u'num_instances': 1,
        u'instance_index': 0,
      }
    ],
  }
  assert result == expected, '\n%s\n%s' % (result, expected)

  progress.update_item('%5d' % index, processing=1)
  try:
    logging.info('collect')
    new_test_keys = swarming.get_task_keys(swarming_url, name)
    if not new_test_keys:
      return 'no_test_keys'
    assert test_keys == new_test_keys, (test_keys, new_test_keys)
    out = [
      output
      for _index, output in swarming.yield_results(
          swarming_url, test_keys, timeout, None, False, None)
    ]
    if not out:
      return 'no_result'
    for item in out:
      item.pop('machine_tag')
      item.pop('machine_id')
      # TODO(maruel): Assert output even when run on a real bot.
      _out_actual = item.pop('output')
      # assert out_actual == swarming_load_test_bot.TASK_OUTPUT, out_actual
    expected = [
      {
        u'config_instance_index': 0,
        u'exit_codes': u'0',
        u'num_config_instances': 1,
      }
    ]
    assert out == expected, '\n%s\n%s' % (out, expected)
    return time.time() - start
  finally:
    progress.update_item('%5d - done' % index, processing=-1, processed=1)


def main():
  colorama.init()
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default='',
      help='Swarming server to use')
  swarming.add_filter_options(parser)
  parser.set_defaults(dimensions=[('os', swarming_load_test_bot.OS_NAME)])

  group = optparse.OptionGroup(parser, 'Load generated')
  group.add_option(
      '-s', '--send-rate', type='float', default=16., metavar='RATE',
      help='Rate (item/s) of sending requests as a float, default: %default')
  group.add_option(
      '-D', '--duration', type='float', default=60., metavar='N',
      help='Duration (s) of the sending phase of the load test, '
           'default: %default')
  group.add_option(
      '-m', '--concurrent', type='int', default=200, metavar='N',
      help='Maximum concurrent on-going requests, default: %default')
  group.add_option(
      '-t', '--timeout', type='float', default=15*60., metavar='N',
      help='Task expiration and timeout to get results, the task itself will '
           'have %ds less than the value provided. Default: %%default' %
               TIMEOUT_OVERHEAD)
  group.add_option(
      '-o', '--output-size', type='int', default=100, metavar='N',
      help='Bytes sent to stdout, default: %default')
  group.add_option(
      '--sleep', type='int', default=60, metavar='N',
      help='Amount of time the bot should sleep, e.g. faking work, '
           'default: %default')
  parser.add_option_group(group)

  group = optparse.OptionGroup(parser, 'Display options')
  group.add_option(
      '--columns', type='int', default=graph.get_console_width(), metavar='N',
      help='For histogram display, default:%default')
  group.add_option(
      '--buckets', type='int', default=20, metavar='N',
      help='Number of buckets for histogram display, default:%default')
  parser.add_option_group(group)

  parser.add_option(
      '--dump', metavar='FOO.JSON', help='Dumps to json file')
  parser.add_option(
      '-v', '--verbose', action='store_true', help='Enables logging')

  options, args = parser.parse_args()
  logging.basicConfig(level=logging.INFO if options.verbose else logging.FATAL)
  if args:
    parser.error('Unsupported args: %s' % args)
  options.swarming = options.swarming.rstrip('/')
  if not options.swarming:
    parser.error('--swarming is required.')
  if options.duration <= 0:
    parser.error('Needs --duration > 0. 0.01 is a valid value.')
  swarming.process_filter_options(parser, options)

  total = int(round(options.send_rate * options.duration))
  print(
      'Sending %.1f i/s for %ds with max %d parallel requests; timeout %.1fs; '
      'total %d' %
        (options.send_rate, options.duration, options.concurrent,
        options.timeout, total))
  print('[processing/processed/todo]')

  # This is used so there's no clash between runs and actual real usage.
  unique = ''.join(random.choice(string.ascii_letters) for _ in range(8))
  columns = [('processing', 0), ('processed', 0), ('todo', 0)]
  progress = threading_utils.Progress(columns)
  index = 0
  results = []
  with threading_utils.ThreadPoolWithProgress(
      progress, 1, options.concurrent, 0) as pool:
    try:
      start = time.time()
      while True:
        duration = time.time() - start
        if duration > options.duration:
          break
        should_have_triggered_so_far = int(round(duration * options.send_rate))
        while index < should_have_triggered_so_far:
          pool.add_task(
              0,
              trigger_task,
              options.swarming,
              options.dimensions,
              options.sleep,
              options.output_size,
              progress,
              unique,
              options.timeout,
              index)
          progress.update_item('', todo=1)
          index += 1
          progress.print_update()
        time.sleep(0.01)
      progress.update_item('Getting results for on-going tasks.', raw=True)
      for i in pool.iter_results():
        results.append(i)
        # This is a bit excessive but it's useful in the case where some tasks
        # hangs, so at least partial data is available.
        if options.dump:
          results.sort()
          if os.path.exists(options.dump):
            os.rename(options.dump, options.dump + '.old')
          with open(options.dump, 'wb') as f:
            json.dump(results, f, separators=(',',':'))
      if not options.dump:
        results.sort()
    except KeyboardInterrupt:
      aborted = pool.abort()
      progress.update_item(
          'Got Ctrl-C. Aborted %d unsent tasks.' % aborted,
          raw=True,
          todo=-aborted)
      progress.print_update()
  progress.print_update()
  # At this point, progress is not used anymore.
  print('')
  print(' - Took %.1fs.' % (time.time() - start))
  print('')
  print_results(results, options.columns, options.buckets)
  return 0


if __name__ == '__main__':
  sys.exit(main())
