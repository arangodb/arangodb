# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import socket

import infra_libs
from infra_libs.event_mon.protos.chrome_infra_log_pb2 import (
  ChromeInfraEvent, ServiceEvent)
from infra_libs.event_mon import router as ev_router

DEFAULT_SERVICE_ACCOUNT_CREDS = 'service-account-event-mon.json'
RUNTYPES = set(('dry', 'test', 'prod', 'file'))

# Remote endpoints
ENDPOINTS = {
  'test': 'https://jmt17.google.com/log',
  'prod': 'https://play.googleapis.com/log',
}

# Instance of router._Router (singleton)
_router = None

# Cache some generally useful values / options
_cache = {}


def add_argparse_options(parser):
  # The default values should make sense for local testing, not production.
  group = parser.add_argument_group('Event monitoring (event_mon) '
                                    'global options')
  group.add_argument('--dry-run', default=False,
                     action='store_true',
                     help='When passed, just print what would happen, but '
                     'do not do it.'
                     )
  group.add_argument('--event-mon-run-type', default='dry',
                      choices=RUNTYPES,
                      help='Determine how to send data. "dry" does not send\n'
                      'anything. "test" sends to the test endpoint, \n'
                      '"prod" to the actual production endpoint, and "file" \n'
                      'writes to a file.')
  group.add_argument('--event-mon-output-file', default='event_mon.output',
                     help='File into which LogEventLite serialized protos are\n'
                     'written when --event-mon-run-type is "file"')
  group.add_argument('--event-mon-service-name',
                      help='Service name to use in log events.')
  group.add_argument('--event-mon-hostname',
                      help='Hostname to use in log events.')
  group.add_argument('--event-mon-appengine-name',
                      help='App name to use in log events.')
  group.add_argument('--event-mon-service-account-creds',
                     default=DEFAULT_SERVICE_ACCOUNT_CREDS,
                     metavar='JSON_FILE',
                     help="Path to a json file containing a service account's"
                     "\ncredentials. This is relative to the path specified\n"
                     "in --event-mon-service-accounts-creds-root\n"
                     "Defaults to '%(default)s'")
  group.add_argument('--event-mon-service-accounts-creds-root',
                     metavar='DIR',
                     default=infra_libs.SERVICE_ACCOUNTS_CREDS_ROOT,
                     help="Directory containing service accounts credentials.\n"
                     "Defaults to %(default)s"
                     )


def process_argparse_options(args):  # pragma: no cover
  """Initializes event monitoring based on provided arguments.

  Args:
    args(argparse.Namespace): output of ArgumentParser.parse_args.
  """
  setup_monitoring(
    run_type=args.event_mon_run_type,
    hostname=args.event_mon_hostname,
    service_name=args.event_mon_service_name,
    appengine_name=args.event_mon_appengine_name,
    service_account_creds=args.event_mon_service_account_creds,
    service_accounts_creds_root=args.event_mon_service_accounts_creds_root,
    output_file=args.event_mon_output_file,
    dry_run=args.dry_run
  )


def setup_monitoring(run_type='dry',
                     hostname=None,
                     service_name=None,
                     appengine_name=None,
                     service_account_creds=None,
                     service_accounts_creds_root=None,
                     output_file=None,
                     dry_run=False):
  """Initializes event monitoring.

  This function is mainly used to provide default global values which are
  required for the module to work.

  If you're implementing a command-line tool, use process_argparse_options
  instead.

  Args:
    run_type (str): One of 'dry', 'test', or 'prod'. Do respectively nothing,
      hit the testing endpoint and the production endpoint.

    hostname (str): hostname as it should appear in the event. If not provided
      a default value is computed.

    service_name (str): logical name of the service that emits events. e.g.
      "commit_queue".

    appengine_name (str): name of the appengine app, if running on appengine.

    service_account_creds (str): path to a json file containing a service
      account's credentials obtained from a Google Cloud project. **Path is
      relative to service_account_creds_root**, which is not the current path by
      default. See infra_libs.authentication for details.

    service_account_creds_root (str): path containing credentials files.

    output_file (str): file where to write the output in run_type == 'file'
      mode.

    dry_run (bool): if True, the code has no side-effect, what would have been
      done is printed instead.
  """
  global _router
  logging.debug('event_mon: setting up monitoring.')

  if not _router:
    default_event = ChromeInfraEvent()

    hostname = hostname or socket.getfqdn()
    # hostname might be empty string or None on some systems, who knows.
    if hostname:  # pragma: no branch
      default_event.event_source.host_name = hostname
    else:  # pragma: no cover
      logging.warning('event_mon: unable to determine hostname.')

    if service_name:
      default_event.event_source.service_name = service_name
    if appengine_name:
      default_event.event_source.appengine_name = appengine_name

    _cache['default_event'] = default_event
    if run_type in ('prod', 'test'):
      _cache['service_account_creds'] = service_account_creds
      _cache['service_accounts_creds_root'] = service_accounts_creds_root
    else:
      _cache['service_account_creds'] = None
      _cache['service_accounts_creds_root'] = None

    if run_type not in RUNTYPES:
      logging.error('Unknown run_type (%s). Setting to "dry"', run_type)
      run_type = 'dry'

    if run_type == 'dry':
      # If we are running on AppEngine or devserver, use logging module.
      server_software = os.environ.get('SERVER_SOFTWARE', '')
      if (server_software.startswith('Google App Engine') or
          server_software.startswith('Development')):
        _router = ev_router._LoggingStreamRouter()
      else:
        _router = ev_router._TextStreamRouter()
    elif run_type == 'file':
      _router = ev_router._LocalFileRouter(output_file,
                                           dry_run=dry_run)
    else:
      _router = ev_router._HttpRouter(_cache,
                                      ENDPOINTS.get(run_type),
                                      dry_run=dry_run)


def get_default_event():
  """Returns a copy of the default event."""

  # We return a copy here to tell people not to modify the event directly.
  ret = ChromeInfraEvent()
  ret.CopyFrom(_cache['default_event'])
  return ret


def set_default_event(event):
  """Change the default ChromeInfraEvent used to compute all events.

  Args:
    event (ChromeInfraEvent): default event
  """
  # Here we raise an exception because failing to set the default event
  # could lead to invalid data in the database.
  if not isinstance(event, ChromeInfraEvent):
    msg = ('A ChromeInfraEvent is required as the default event. Got %s' %
           type(event))
    logging.error(msg)
    raise TypeError(msg)

  _cache['default_event'] = event


def close():
  """Reset the state.

  Call this right before exiting the program.

  Returns:
    success (bool): False if an error occured
  """
  global _router, _cache
  _router = None
  _cache = {}
  return True
