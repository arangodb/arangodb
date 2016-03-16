# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import copy
import json
import logging
import os
import re
import socket
import sys
import time

import httplib2
import oauth2client.client

from infra_libs.ts_mon.common import http_metrics

DEFAULT_SCOPES = ['email']

# default timeout for http requests, in seconds
DEFAULT_TIMEOUT = 30

# This is part of the API.
if sys.platform.startswith('win'): # pragma: no cover
  SERVICE_ACCOUNTS_CREDS_ROOT = 'C:\\creds\\service_accounts'
else:
  SERVICE_ACCOUNTS_CREDS_ROOT = '/creds/service_accounts'


class AuthError(Exception):
  pass


def load_service_account_credentials(credentials_filename,
                                     service_accounts_creds_root=None):
  """Loads and validate a credential JSON file.

  Example of a well-formatted file:
    {
      "private_key_id": "4168d274cdc7a1eaef1c59f5b34bdf255",
      "private_key": ("-----BEGIN PRIVATE KEY-----\nMIIhkiG9w0BAQEFAASCAmEwsd"
                      "sdfsfFd\ngfxFChctlOdTNm2Wrr919Nx9q+sPV5ibyaQt5Dgn89fKV"
                      "jftrO3AMDS3sMjaE4Ib\nZwJgy90wwBbMT7/YOzCgf5PZfivUe8KkB"
                      -----END PRIVATE KEY-----\n",
      "client_email": "234243-rjstu8hi95iglc8at3@developer.gserviceaccount.com",
      "client_id": "234243-rjstu8hi95iglc8at3.apps.googleusercontent.com",
      "type": "service_account"
    }

  Args:
    credentials_filename (str): path to a .json file containing credentials
      for a Cloud platform service account.

  Keyword Args:
    service_accounts_creds_root (str or None): location where all service
      account credentials are stored. ``credentials_filename`` is relative
      to this path. None means 'use default location'.

  Raises:
    AuthError: if the file content is invalid.
  """
  service_accounts_creds_root = (service_accounts_creds_root
                                 or SERVICE_ACCOUNTS_CREDS_ROOT)

  service_account_file = os.path.join(service_accounts_creds_root,
                                      credentials_filename)
  try:
    with open(service_account_file, 'r') as f:
      key = json.load(f)
  except ValueError as e:
    raise AuthError('Parsing of file as JSON failed (%s): %s',
                    e, service_account_file)

  if key.get('type') != 'service_account':
    msg = ('Credentials type must be for a service_account, got %s.'
           ' Check content of %s' % (key.get('type'), service_account_file))
    logging.error(msg)
    raise AuthError(msg)

  if not key.get('client_email'):
    msg = ('client_email field missing in credentials json file. '
           ' Check content of %s' % service_account_file)
    logging.error(msg)
    raise AuthError(msg)

  if not key.get('private_key'):
    msg = ('private_key field missing in credentials json. '
           ' Check content of %s' % service_account_file)
    logging.error(msg)
    raise AuthError(msg)

  return key


def get_signed_jwt_assertion_credentials(credentials_filename,
                                         scope=None,
                                         service_accounts_creds_root=None):
  """Factory for SignedJwtAssertionCredentials

  Reads and validate the json credential file.

  Args:
    credentials_filename (str): path to the service account key file.
      See load_service_account_credentials() docstring for the file format.

  Keyword Args:
    scope (str|list of str): scope(s) of the credentials being
      requested. Defaults to https://www.googleapis.com/auth/userinfo.email.
    service_accounts_creds_root (str or None): location where all service
      account credentials are stored. ``credentials_filename`` is relative
      to this path. None means 'use default location'.
  """
  scope = scope or DEFAULT_SCOPES
  if isinstance(scope, basestring):
    scope = [scope]
  assert all(isinstance(s, basestring) for s in scope)

  key = load_service_account_credentials(
    credentials_filename,
    service_accounts_creds_root=service_accounts_creds_root)

  return oauth2client.client.SignedJwtAssertionCredentials(
    key['client_email'], key['private_key'], scope)


def get_authenticated_http(credentials_filename,
                           scope=None,
                           service_accounts_creds_root=None,
                           http_identifier=None,
                           timeout=DEFAULT_TIMEOUT):
  """Creates an httplib2.Http wrapped with a service account authenticator.

  Args:
    credentials_filename (str): relative path to the file containing
      credentials in json format. Path is relative to the default
      location where credentials are stored (platform-dependent).

  Keyword Args:
    scope (str|list of str): scope(s) of the credentials being
      requested. Defaults to https://www.googleapis.com/auth/userinfo.email.
    service_accounts_creds_root (str or None): location where all service
      account credentials are stored. ``credentials_filename`` is relative
      to this path. None means 'use default location'.
    http_identifier (str): if provided, returns an instrumented http request
      and use this string to identify it to ts_mon.
    timeout (int): timeout passed to httplib2.Http, in seconds.

  Returns:
    httplib2.Http authenticated with master's service account.
  """
  creds = get_signed_jwt_assertion_credentials(
    credentials_filename,
    scope=scope,
    service_accounts_creds_root=service_accounts_creds_root)

  if http_identifier:
    http = InstrumentedHttp(http_identifier, timeout=timeout)
  else:
    http = httplib2.Http(timeout=timeout)
  return creds.authorize(http)


class InstrumentedHttp(httplib2.Http):
  """A httplib2.Http object that reports ts_mon metrics about its requests."""

  def __init__(self, name, time_fn=time.time, timeout=DEFAULT_TIMEOUT,
               **kwargs):
    """
    Args:
      name: An identifier for the HTTP requests made by this object.
      time_fn: Function returning the current time in seconds. Use for testing
        purposes only.
    """

    super(InstrumentedHttp, self).__init__(timeout=timeout, **kwargs)
    self.fields = {'name': name, 'client': 'httplib2'}
    self.time_fn = time_fn

  def _update_metrics(self, status, start_time):
    status_fields = {'status': status}
    status_fields.update(self.fields)
    http_metrics.response_status.increment(fields=status_fields)

    duration_msec = (self.time_fn() - start_time) * 1000
    http_metrics.durations.add(duration_msec, fields=self.fields)

  def request(self, uri, method="GET", body=None, *args, **kwargs):
    request_bytes = 0
    if body is not None:
      request_bytes = len(body)
    http_metrics.request_bytes.add(request_bytes, fields=self.fields)

    start_time = self.time_fn()
    try:
      response, content = super(InstrumentedHttp, self).request(
          uri, method, body, *args, **kwargs)
    except socket.timeout:
      self._update_metrics(http_metrics.STATUS_TIMEOUT, start_time)
      raise
    except socket.error:
      self._update_metrics(http_metrics.STATUS_ERROR, start_time)
      raise
    except httplib2.HttpLib2Error:
      self._update_metrics(http_metrics.STATUS_EXCEPTION, start_time)
      raise
    http_metrics.response_bytes.add(len(content), fields=self.fields)

    self._update_metrics(response.status, start_time)

    return response, content


class HttpMock(object):
  """Mock of httplib2.Http"""
  HttpCall = collections.namedtuple('HttpCall', ('uri', 'method', 'body',
                                                 'headers'))

  def __init__(self, uris):
    """
    Args:
      uris(dict): list of  (uri, headers, body). `uri` is a regexp for
        matching the requested uri, (headers, body) gives the values returned
        by the mock. Uris are tested in the order from `uris`.
        `headers` is a dict mapping headers to value. The 'status' key is
        mandatory. `body` is a string.
        Ex: [('.*', {'status': 200}, 'nicely done.')]
    """
    self._uris = []
    self.requests_made = []

    for value in uris:
      if not isinstance(value, (list, tuple)) or len(value) != 3:
        raise ValueError("'uris' must be a sequence of (uri, headers, body)")
      uri, headers, body = value
      compiled_uri = re.compile(uri)
      if not isinstance(headers, dict):
        raise TypeError("'headers' must be a dict")
      if not 'status' in headers:
        raise ValueError("'headers' must have 'status' as a key")

      new_headers = copy.copy(headers)
      new_headers['status'] = int(new_headers['status'])

      if not isinstance(body, basestring):
        raise TypeError("'body' must be a string, got %s" % type(body))
      self._uris.append((compiled_uri, new_headers, body))

  # pylint: disable=unused-argument
  def request(self, uri,
              method='GET',
              body=None,
              headers=None,
              redirections=1,
              connection_type=None):
    self.requests_made.append(self.HttpCall(uri, method, body, headers))
    headers = None
    body = None
    for candidate in self._uris:
      if candidate[0].match(uri):
        _, headers, body = candidate
        break
    if not headers:
      raise AssertionError("Unexpected request to %s" % uri)
    return httplib2.Response(headers), body
