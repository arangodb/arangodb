#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Triggers a ton of fake jobs to test its handling under high load.

Generates an histogram with the latencies to process the tasks and number of
retries.
"""

import hashlib
import json
import logging
import optparse
import os
import Queue
import socket
import StringIO
import sys
import threading
import time
import zipfile

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sys.path.insert(0, ROOT_DIR)

from third_party import colorama

import swarming

from utils import graph
from utils import net
from utils import threading_utils

# Line too long (NN/80)
# pylint: disable=C0301

OS_NAME = 'Comodore64'
TASK_OUTPUT = 'This task ran with great success'


def print_results(results, columns, buckets):
  delays = [i for i in results if isinstance(i, float)]
  failures = [i for i in results if not isinstance(i, float)]

  print('%sDELAYS%s:' % (colorama.Fore.RED, colorama.Fore.RESET))
  graph.print_histogram(
      graph.generate_histogram(delays, buckets), columns, ' %.3f')
  print('')
  print('Total items  : %d' % len(results))
  average = 0
  if delays:
    average = sum(delays)/ len(delays)
  print('Average delay: %s' % graph.to_units(average))
  print('')

  if failures:
    print('%sEVENTS%s:' % (colorama.Fore.RED, colorama.Fore.RESET))
    values = {}
    for f in failures:
      values.setdefault(f, 0)
      values[f] += 1
    graph.print_histogram(values, columns, ' %s')
    print('')


def generate_version(source):
  """Generates the sha-1 based on the content of this zip.

  Copied from:
  https://code.google.com/p/swarming/source/browse/services/swarming/swarm_bot/zipped_archive.py
  """
  result = hashlib.sha1()
  with zipfile.ZipFile(source, 'r') as z:
    for item in sorted(z.namelist()):
      with z.open(item) as f:
        result.update(item)
        result.update('\x00')
        result.update(f.read())
        result.update('\x00')
  return result.hexdigest()


def calculate_version(url):
  """Retrieves the swarm_bot code and returns the SHA-1 for it."""
  # Cannot use url_open() since zipfile requires .seek().
  return generate_version(StringIO.StringIO(net.url_read(url)))


def get_hostname():
  return socket.getfqdn().lower().split('.', 1)[0]


class FakeSwarmBot(object):
  """This is a Fake swarm_bot implementation simulating it is running
  Comodore64.

  It polls for job, acts as if it was processing them and return the fake
  result.
  """
  def __init__(
      self, swarming_url, dimensions, swarm_bot_version_hash, hostname, index,
      progress, duration, events, kill_event):
    self._lock = threading.Lock()
    self._swarming = swarming_url
    self._index = index
    self._progress = progress
    self._duration = duration
    self._events = events
    self._kill_event = kill_event
    self._bot_id = '%s-%d' % (hostname, index)
    self._attributes = {
      'dimensions': dimensions,
      'id': self._bot_id,
      # TODO(maruel): Use os_utilities.py.
      'ip': '127.0.0.1',
      'try_count': 0,
      'version': swarm_bot_version_hash,
    }

    self._thread = threading.Thread(target=self._run, name='bot%d' % index)
    self._thread.daemon = True
    self._thread.start()

  def join(self):
    self._thread.join()

  def is_alive(self):
    return self._thread.is_alive()

  def _run(self):
    """Polls the server and fake execution."""
    try:
      self._progress.update_item('%d alive' % self._index, bots=1)
      while True:
        if self._kill_event.is_set():
          return
        data = {'attributes': json.dumps(self._attributes)}
        request = net.url_open(self._swarming + '/poll_for_test', data=data)
        if request is None:
          self._events.put('poll_for_test_empty')
          continue
        start = time.time()
        try:
          manifest = json.load(request)
        except ValueError:
          self._progress.update_item('Failed to poll')
          self._events.put('poll_for_test_invalid')
          continue

        commands = [c['function'] for c in manifest.get('commands', [])]
        if not commands:
          # Nothing to run.
          self._events.put('sleep')
          time.sleep(manifest['come_back'])
          continue

        if commands == ['UpdateSlave']:
          # Calculate the proper SHA-1 and loop again.
          # This could happen if the Swarming server is upgraded while this
          # script runs.
          self._attributes['version'] = calculate_version(
              manifest['commands'][0]['args'])
          self._events.put('update_slave')
          continue

        if commands != ['RunManifest']:
          self._progress.update_item(
              'Unexpected RPC call %s\n%s' % (commands, manifest))
          self._events.put('unknown_rpc')
          break

        store_cmd = manifest['commands'][0]
        if not isinstance(store_cmd['args'], unicode):
          self._progress.update_item('Unexpected RPC manifest\n%s' % manifest)
          self._events.put('unknown_args')
          break

        result_url = manifest['result_url']
        test_run = json.loads(store_cmd['args'])
        if result_url != test_run['result_url']:
          self._progress.update_item(
              'Unexpected result url: %s != %s' %
              (result_url, test_run['result_url']))
          self._events.put('invalid_result_url')
          break
        ping_url = test_run['ping_url']
        ping_delay = test_run['ping_delay']
        self._progress.update_item('%d processing' % self._index, processing=1)

        # Fake activity and send pings as requested.
        while True:
          remaining = max(0, (start + self._duration) - time.time())
          if remaining > ping_delay:
            # Include empty data to ensure the request is a POST request.
            result = net.url_read(ping_url, data={})
            assert result == 'Success.', result
            remaining = max(0, (start + self._duration) - time.time())
          if not remaining:
            break
          time.sleep(remaining)

        # In the old API, r=<task_id>&id=<bot_id> is passed as the url.
        data = {
          'o': TASK_OUTPUT,
          'x': '0',
        }
        result = net.url_read(manifest['result_url'], data=data)
        self._progress.update_item(
            '%d processed' % self._index, processing=-1, processed=1)
        if not result:
          self._events.put('result_url_fail')
        else:
          assert result == 'Successfully update the runner results.', result
          self._events.put(time.time() - start)
    finally:
      try:
        # Unregister itself. Otherwise the server will have tons of fake slaves
        # that the admin will have to remove manually.
        response = net.url_open(
            self._swarming + '/delete_machine_stats',
            data=[('r', self._bot_id)])
        if not response:
          self._events.put('failed_unregister')
        else:
          response.read()
      finally:
        self._progress.update_item('%d quit' % self._index, bots=-1)


def main():
  colorama.init()
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default='',
      help='Swarming server to use')
  parser.add_option(
      '--suffix', metavar='NAME', default='', help='Bot suffix name to use')
  swarming.add_filter_options(parser)
  # Use improbable values to reduce the chance of interferring with real slaves.
  parser.set_defaults(
      dimensions=[
        ('cpu', ['arm36']),
        ('hostname', socket.getfqdn()),
        ('os', OS_NAME),
      ])

  group = optparse.OptionGroup(parser, 'Load generated')
  group.add_option(
      '--slaves', type='int', default=300, metavar='N',
      help='Number of swarm bot slaves, default: %default')
  group.add_option(
      '-c', '--consume', type='float', default=60., metavar='N',
      help='Duration (s) for consuming a request, default: %default')
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
  if options.consume <= 0:
    parser.error('Needs --consume > 0. 0.01 is a valid value.')
  swarming.process_filter_options(parser, options)

  print(
      'Running %d slaves, each task lasting %.1fs' % (
        options.slaves, options.consume))
  print('Ctrl-C to exit.')
  print('[processing/processed/bots]')
  columns = [('processing', 0), ('processed', 0), ('bots', 0)]
  progress = threading_utils.Progress(columns)
  events = Queue.Queue()
  start = time.time()
  kill_event = threading.Event()
  swarm_bot_version_hash = calculate_version(options.swarming + '/bot_code')
  hostname = get_hostname()
  if options.suffix:
    hostname += '-' + options.suffix
  slaves = [
    FakeSwarmBot(
      options.swarming, options.dimensions, swarm_bot_version_hash, hostname, i,
      progress, options.consume, events, kill_event)
    for i in range(options.slaves)
  ]
  try:
    # Wait for all the slaves to come alive.
    while not all(s.is_alive() for s in slaves):
      time.sleep(0.01)
    progress.update_item('Ready to run')
    while slaves:
      progress.print_update()
      time.sleep(0.01)
      # The slaves could be told to die.
      slaves = [s for s in slaves if s.is_alive()]
  except KeyboardInterrupt:
    kill_event.set()

  progress.update_item('Waiting for slaves to quit.', raw=True)
  progress.update_item('')
  while slaves:
    progress.print_update()
    slaves = [s for s in slaves if s.is_alive()]
  # At this point, progress is not used anymore.
  print('')
  print('Ran for %.1fs.' % (time.time() - start))
  print('')
  results = list(events.queue)
  print_results(results, options.columns, options.buckets)
  if options.dump:
    with open(options.dump, 'w') as f:
      json.dump(results, f, separators=(',',':'))
  return 0


if __name__ == '__main__':
  sys.exit(main())
