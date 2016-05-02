#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Client tool to perform various authentication related tasks."""

__version__ = '0.4'

import logging
import optparse
import sys

from third_party import colorama
from third_party.depot_tools import fix_encoding
from third_party.depot_tools import subcommand

from utils import logging_utils
from utils import on_error
from utils import net
from utils import oauth
from utils import tools


class AuthServiceError(Exception):
  """Unexpected response from authentication service."""


class AuthService(object):
  """Represents remote Authentication service."""

  def __init__(self, url):
    self._service = net.get_http_service(url)

  def login(self, allow_user_interaction):
    """Refreshes cached access token or creates a new one."""
    return self._service.login(allow_user_interaction)

  def logout(self):
    """Purges cached access token."""
    return self._service.logout()

  def get_current_identity(self):
    """Returns identity associated with currently used credentials.

    Identity is a string:
      user:<email> - if using OAuth or cookie based authentication.
      bot:<id> - if using HMAC based authentication.
      anonymous:anonymous - if not authenticated.
    """
    identity = self._service.json_request('/auth/api/v1/accounts/self')
    if not identity:
      raise AuthServiceError('Failed to fetch identity')
    return identity['identity']


def add_auth_options(parser):
  """Adds command line options related to authentication."""
  parser.auth_group = optparse.OptionGroup(parser, 'Authentication')
  parser.add_option_group(parser.auth_group)
  oauth.add_oauth_options(parser)


def process_auth_options(parser, options):
  """Configures process-wide authentication parameters based on |options|."""
  try:
    net.set_oauth_config(oauth.extract_oauth_config_from_options(options))
  except ValueError as exc:
    parser.error(str(exc))


def normalize_host_url(url):
  """Makes sure URL starts with http:// or https://."""
  url = url.lower().rstrip('/')
  if url.startswith('https://'):
    return url
  if url.startswith('http://'):
    allowed = ('http://localhost:', 'http://127.0.0.1:', 'http://::1:')
    if not url.startswith(allowed):
      raise ValueError(
          'URL must start with https:// or be on localhost with port number')
    return url
  return 'https://' + url


def ensure_logged_in(server_url):
  """Checks that user is logged in, asking to do it if not.

  Raises:
    ValueError if the server_url is not acceptable.
  """
  # It's just a waste of time on a headless bot (it can't do interactive login).
  if tools.is_headless() or net.get_oauth_config().disabled:
    return None
  server_url = normalize_host_url(server_url)
  service = AuthService(server_url)
  try:
    service.login(False)
  except IOError:
    raise ValueError('Failed to contact %s' % server_url)
  try:
    identity = service.get_current_identity()
  except AuthServiceError:
    raise ValueError('Failed to fetch identify from %s' % server_url)
  if identity == 'anonymous:anonymous':
    raise ValueError(
        'Please login to %s: \n'
        '  python auth.py login --service=%s' % (server_url, server_url))
  email = identity.split(':')[1]
  logging.info('Logged in to %s: %s', server_url, email)
  return email


@subcommand.usage('[options]')
def CMDlogin(parser, args):
  """Runs interactive login flow and stores auth token/cookie on disk."""
  (options, args) = parser.parse_args(args)
  process_auth_options(parser, options)
  service = AuthService(options.service)
  if service.login(True):
    print 'Logged in as \'%s\'.' % service.get_current_identity()
    return 0
  else:
    print 'Login failed or canceled.'
    return 1


@subcommand.usage('[options]')
def CMDlogout(parser, args):
  """Purges cached auth token/cookie."""
  (options, args) = parser.parse_args(args)
  process_auth_options(parser, options)
  service = AuthService(options.service)
  service.logout()
  return 0


@subcommand.usage('[options]')
def CMDcheck(parser, args):
  """Shows identity associated with currently cached auth token/cookie."""
  (options, args) = parser.parse_args(args)
  process_auth_options(parser, options)
  service = AuthService(options.service)
  service.login(False)
  print service.get_current_identity()
  return 0


class OptionParserAuth(logging_utils.OptionParserWithLogging):
  def __init__(self, **kwargs):
    logging_utils.OptionParserWithLogging.__init__(
        self, prog='auth.py', **kwargs)
    self.server_group = optparse.OptionGroup(self, 'Server')
    self.server_group.add_option(
        '-S', '--service',
        metavar='URL', default='',
        help='Service to use')
    self.add_option_group(self.server_group)
    add_auth_options(self)

  def parse_args(self, *args, **kwargs):
    options, args = logging_utils.OptionParserWithLogging.parse_args(
        self, *args, **kwargs)
    if not options.service:
      self.error('--service is required.')
    try:
      options.service = normalize_host_url(options.service)
    except ValueError as exc:
      self.error(str(exc))
    on_error.report_on_exception_exit(options.service)
    return options, args


def main(args):
  dispatcher = subcommand.CommandDispatcher(__name__)
  return dispatcher.execute(OptionParserAuth(version=__version__), args)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  tools.disable_buffering()
  colorama.init()
  sys.exit(main(sys.argv[1:]))
