# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import socket
import sys
import urlparse
import re

import requests

from infra_libs.ts_mon.common import interface
from infra_libs.ts_mon.common import metric_store
from infra_libs.ts_mon.common import monitors
from infra_libs.ts_mon.common import standard_metrics
from infra_libs.ts_mon.common import targets


def load_machine_config(filename):
  if not os.path.exists(filename):
    logging.info('Configuration file does not exist, ignoring: %s', filename)
    return {}

  try:
    with open(filename) as fh:
      return json.load(fh)
  except Exception:
    logging.error('Configuration file couldn\'t be read: %s', filename)
    raise


def _default_region(fqdn):
  # Check if we're running in a GCE instance.
  try:
    r = requests.get(
        'http://metadata.google.internal/computeMetadata/v1/instance/zone',
        headers={'Metadata-Flavor': 'Google'},
        timeout=1.0)
  except requests.exceptions.RequestException:
    pass
  else:
    if r.status_code == requests.codes.ok:
      # The zone is the last slash-separated component.
      return r.text.split('/')[-1]

  try:
    return fqdn.split('.')[1]  # [chrome|golo]
  except IndexError:
    return ''


def _default_network(host):
  try:
    # Regular expression that matches the vast majority of our host names.
    # Matches everything of the form 'masterN', 'masterNa', and 'foo-xN'.
    return re.match(r'^([\w-]*?-[acm]|master)(\d+)a?$', host).group(2)  # N
  except AttributeError:
    return ''


def add_argparse_options(parser):
  """Add monitoring related flags to a process' argument parser.

  Args:
    parser (argparse.ArgumentParser): the parser for the main process.
  """
  if sys.platform == 'win32':  # pragma: no cover
    default_config_file = 'C:\\chrome-infra\\ts-mon.json'
  else:  # pragma: no cover
    default_config_file = '/etc/chrome-infra/ts-mon.json'

  parser = parser.add_argument_group('Timeseries Monitoring Options')
  parser.add_argument(
      '--ts-mon-config-file',
      default=default_config_file,
      help='path to a JSON config file that contains suitable values for '
           '"endpoint" and "credentials" for this machine. This config file is '
           'intended to be shared by all processes on the machine, as the '
           'values depend on the machine\'s position in the network, IP '
           'whitelisting and deployment of credentials. (default: %(default)s)')
  parser.add_argument(
      '--ts-mon-endpoint',
      help='url (including file://, pubsub://project/topic) to post monitoring '
           'metrics to. If set, overrides the value in --ts-mon-config-file')
  parser.add_argument(
      '--ts-mon-credentials',
      help='path to a pkcs8 json credential file. If set, overrides the value '
           'in --ts-mon-config-file')
  parser.add_argument(
      '--ts-mon-flush',
      choices=('manual', 'auto'), default='auto',
      help=('metric push behavior: manual (only send when flush() is called), '
            'or auto (send automatically every --ts-mon-flush-interval-secs '
            'seconds). (default: %(default)s)'))
  parser.add_argument(
      '--ts-mon-flush-interval-secs',
      type=int,
      default=60,
      help=('automatically push metrics on this interval if '
            '--ts-mon-flush=auto.'))

  parser.add_argument(
      '--ts-mon-target-type',
      choices=('device', 'task'),
      default='device',
      help='the type of target that is being monitored ("device" or "task").'
           ' (default: %(default)s)')

  fqdn = socket.getfqdn().lower()  # foo-[a|m]N.[chrome|golo].chromium.org
  host = fqdn.split('.')[0]  # foo-[a|m]N
  region = _default_region(fqdn)
  network = _default_network(host)

  parser.add_argument(
      '--ts-mon-device-hostname',
      default=host,
      help='name of this device, (default: %(default)s)')
  parser.add_argument(
      '--ts-mon-device-region',
      default=region,
      help='name of the region this devices lives in. (default: %(default)s)')
  parser.add_argument(
      '--ts-mon-device-role',
      default='default',
      help='Role of the device. (default: %(default)s)')
  parser.add_argument(
      '--ts-mon-device-network',
      default=network,
      help='name of the network this device is connected to. '
           '(default: %(default)s)')

  parser.add_argument(
      '--ts-mon-task-service-name',
      help='name of the service being monitored')
  parser.add_argument(
      '--ts-mon-task-job-name',
      help='name of this job instance of the task')
  parser.add_argument(
      '--ts-mon-task-region',
      default=region,
      help='name of the region in which this task is running '
           '(default: %(default)s)')
  parser.add_argument(
      '--ts-mon-task-hostname',
      default=host,
      help='name of the host on which this task is running '
           '(default: %(default)s)')
  parser.add_argument(
      '--ts-mon-task-number', type=int, default=0,
      help='number (e.g. for replication) of this instance of this task '
           '(default: %(default)s)')


def process_argparse_options(args):
  """Process command line arguments to initialize the global monitor.

  Also initializes the default target.

  Starts a background thread to automatically flush monitoring metrics if not
  disabled by command line arguments.

  Args:
    args (argparse.Namespace): the result of parsing the command line arguments
  """
  # Parse the config file if it exists.
  config = load_machine_config(args.ts_mon_config_file)
  endpoint = config.get('endpoint', '')
  credentials = config.get('credentials', '')

  # Command-line args override the values in the config file.
  if args.ts_mon_endpoint is not None:
    endpoint = args.ts_mon_endpoint
  if args.ts_mon_credentials is not None:
    credentials = args.ts_mon_credentials

  if args.ts_mon_target_type == 'device':
    interface.state.target = targets.DeviceTarget(
        args.ts_mon_device_region,
        args.ts_mon_device_role,
        args.ts_mon_device_network,
        args.ts_mon_device_hostname)
  if args.ts_mon_target_type == 'task':  # pragma: no cover
    # Reimplement ArgumentParser.error, since we don't have access to the parser
    if not args.ts_mon_task_service_name:
      print >> sys.stderr, ('Argument --ts-mon-task-service-name must be '
                            'provided when the target type is "task".')
      sys.exit(2)
    if not args.ts_mon_task_job_name:  # pragma: no cover
      print >> sys.stderr, ('Argument --ts-mon-task-job-name must be provided '
                            'when the target type is "task".')
      sys.exit(2)
    interface.state.target = targets.TaskTarget(
        args.ts_mon_task_service_name,
        args.ts_mon_task_job_name,
        args.ts_mon_task_region,
        args.ts_mon_task_hostname,
        args.ts_mon_task_number)

  interface.state.global_monitor = monitors.NullMonitor()

  if endpoint.startswith('file://'):
    interface.state.global_monitor = monitors.DebugMonitor(
        endpoint[len('file://'):])
  elif endpoint.startswith('pubsub://'):
    if credentials:
      url = urlparse.urlparse(endpoint)
      project = url.netloc
      topic = url.path.strip('/')
      interface.state.global_monitor = monitors.PubSubMonitor(
          credentials, project, topic, use_instrumented_http=True)
    else:
      logging.error('ts_mon monitoring is disabled because credentials are not '
                    'available')
  elif endpoint.lower() == 'none':
    logging.info('ts_mon monitoring has been explicitly disabled')
  else:
    logging.error('ts_mon monitoring is disabled because the endpoint provided'
                  ' is invalid or not supported: %s', endpoint)

  interface.state.flush_mode = args.ts_mon_flush

  if args.ts_mon_flush == 'auto':
    interface.state.flush_thread = interface._FlushThread(
        args.ts_mon_flush_interval_secs)
    interface.state.flush_thread.start()

  standard_metrics.init()
