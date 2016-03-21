# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import sys
import threading

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from utils import net


class TestCase(auto_stub.TestCase):
  """Mocks out url_open() calls."""
  def setUp(self):
    super(TestCase, self).setUp()
    self.mock(net, 'url_open', self._url_open)
    self.mock(net, 'url_read_json', self._url_read_json)
    self.mock(net, 'sleep_before_retry', lambda *_: None)
    self._lock = threading.Lock()
    self._requests = []

  def tearDown(self):
    try:
      if not self.has_failed():
        self.assertEqual([], self._requests)
    finally:
      super(TestCase, self).tearDown()

  def expected_requests(self, requests):
    """Registers the expected requests along their reponses.

    Arguments:
      request: list of tuple(url, kwargs, response, headers) for normal requests
          and tuple(url, kwargs, response) for json requests. kwargs can be a
          callable. In that case, it's called with the actual kwargs. It's
          useful when the kwargs values are not deterministic.
    """
    requests = requests[:]
    for request in requests:
      self.assertEqual(tuple, request.__class__)
      # 3 = json request (url_read_json).
      # 4 = normal request (url_open).
      self.assertIn(len(request), (3, 4))

    with self._lock:
      self.assertEqual([], self._requests)
      self._requests = requests

  def _url_open(self, url, **kwargs):
    logging.warn('url_open(%s, %s)', url[:500], str(kwargs)[:500])
    with self._lock:
      if not self._requests:
        return None
      # Ignore 'stream' argument, it's not important for these tests.
      kwargs.pop('stream', None)
      for i, n in enumerate(self._requests):
        if n[0] == url:
          data = self._requests.pop(i)
          if len(data) != 4:
            self.fail('Expected normal request, got json data; %s' % url)
          _, expected_kwargs, result, headers = data
          if callable(expected_kwargs):
            expected_kwargs(kwargs)
          else:
            self.assertEqual(expected_kwargs, kwargs)
          if result is not None:
            return net.HttpResponse.get_fake_response(result, url, headers)
          return None
    self.fail('Unknown request %s' % url)

  def _url_read_json(self, url, **kwargs):
    logging.warn('url_read_json(%s, %s)', url[:500], str(kwargs)[:500])
    with self._lock:
      if not self._requests:
        return None
      # Ignore 'stream' argument, it's not important for these tests.
      kwargs.pop('stream', None)
      for i, n in enumerate(self._requests):
        if n[0] == url:
          data = self._requests.pop(i)
          if len(data) != 3:
            self.fail('Expected json request, got normal data; %s' % url)
          _, expected_kwargs, result = data
          if callable(expected_kwargs):
            expected_kwargs(kwargs)
          else:
            self.assertEqual(expected_kwargs, kwargs)
          if result is not None:
            return result
          return None
    self.fail('Unknown request %s' % url)
