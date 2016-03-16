# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Report ts_mon metrics for HTTP requests made by the `requests` library.

This module provides get(), post(), etc. methods that wrap the corresponding
methods in the requests module.  They take an additional first 'name' argument
which is an identifier for the type of request being made.

Example::

  from infra_libs import instrumented_requests
  r = instrumented_requests.get('myapi', 'https://example.com/api')

Alternatively you can add the hook manually::

  import requests
  from infra_libs import instrumented_requests
  r = requests.get(
      'https://example.com/api',
      hooks={'response': instrumented_requests.instrumentation_hook('myapi')})
"""

import functools

import requests

from infra_libs.ts_mon.common import http_metrics


def instrumentation_hook(name):
  """Returns a hook function that records ts_mon metrics about the request.

  Usage::

    r = requests.get(
        'https://example.com/api',
        hooks={'response': instrumented_requests.instrumentation_hook('myapi')})

  Args:
    name: An identifier for the HTTP requests made by this object.
  """

  def _content_length(headers):
    if headers is None or 'content-length' not in headers:
      return 0
    return int(headers['content-length'])

  def hook(response, *_args, **_kwargs):
    request_bytes = _content_length(response.request.headers)
    response_bytes = _content_length(response.headers)
    duration_msec = response.elapsed.total_seconds() * 1000

    fields = {'name': name, 'client': 'requests'}
    http_metrics.request_bytes.add(request_bytes, fields=fields)
    http_metrics.response_bytes.add(response_bytes, fields=fields)
    http_metrics.durations.add(duration_msec, fields=fields)

    _update_status(name, response.status_code)

  return hook


def _update_status(name, status):
  fields = {'status': status, 'name': name, 'client': 'requests'}
  http_metrics.response_status.increment(fields=fields)


def _wrap(method, name, url, *args, **kwargs):
  hooks = {'response': instrumentation_hook(name)}
  if 'hooks' in kwargs:
    hooks.update(kwargs['hooks'])
  kwargs['hooks'] = hooks

  try:
    return getattr(requests, method)(url, *args, **kwargs)
  except requests.exceptions.ReadTimeout:
    _update_status(name, http_metrics.STATUS_TIMEOUT)
    raise
  except requests.exceptions.ConnectionError:
    _update_status(name, http_metrics.STATUS_ERROR)
    raise
  except requests.exceptions.RequestException:
    _update_status(name, http_metrics.STATUS_EXCEPTION)
    raise


request = functools.partial(_wrap, 'request')
get = functools.partial(_wrap, 'get')
head = functools.partial(_wrap, 'head')
post = functools.partial(_wrap, 'post')
patch = functools.partial(_wrap, 'patch')
put = functools.partial(_wrap, 'put')
delete = functools.partial(_wrap, 'delete')
options = functools.partial(_wrap, 'options')
