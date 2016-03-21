#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# pylint: disable=R0201,W0613

import StringIO
import __builtin__
import contextlib
import logging
import math
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from utils import net


class RetryLoopMockedTest(auto_stub.TestCase):
  """Base class for test cases that mock retry loop."""

  def setUp(self):
    super(RetryLoopMockedTest, self).setUp()
    self._retry_attemps_cls = net.RetryAttempt
    self.mock(net, 'sleep_before_retry', self.mocked_sleep_before_retry)
    self.mock(net, 'current_time', self.mocked_current_time)
    self.mock(net, 'RetryAttempt', self.mocked_retry_attempt)
    self.sleeps = []
    self.attempts = []

  def mocked_sleep_before_retry(self, attempt, max_wait):
    self.sleeps.append((attempt, max_wait))

  def mocked_current_time(self):
    # One attempt is one virtual second.
    return float(len(self.attempts))

  def mocked_retry_attempt(self, *args, **kwargs):
    attempt = self._retry_attemps_cls(*args, **kwargs)
    self.attempts.append(attempt)
    return attempt

  def assertAttempts(self, attempts, max_timeout):
    """Asserts that retry loop executed given number of |attempts|."""
    expected = [(i, max_timeout - i) for i in xrange(attempts)]
    actual = [(x.attempt, x.remaining) for x in self.attempts]
    self.assertEqual(expected, actual)

  def assertSleeps(self, sleeps):
    """Asserts that retry loop slept given number of times."""
    self.assertEqual(sleeps, len(self.sleeps))


class RetryLoopTest(RetryLoopMockedTest):
  """Test for retry_loop implementation."""

  def test_sleep_before_retry(self):
    # Verifies bounds. Because it's using a pseudo-random number generator and
    # not a read random source, it's basically guaranteed to never return the
    # same value twice consecutively.
    a = net.calculate_sleep_before_retry(0, 0)
    b = net.calculate_sleep_before_retry(0, 0)
    self.assertTrue(a >= math.pow(1.5, -1), a)
    self.assertTrue(b >= math.pow(1.5, -1), b)
    self.assertTrue(a < 1.5 + math.pow(1.5, -1), a)
    self.assertTrue(b < 1.5 + math.pow(1.5, -1), b)
    self.assertNotEqual(a, b)


class HttpServiceTest(RetryLoopMockedTest):
  """Tests for HttpService class."""

  @staticmethod
  def mocked_http_service(
      url='http://example.com',
      perform_request=None,
      authorize=None,
      login=None):

    class MockedAuthenticator(net.Authenticator):
      def authorize(self, request):
        return authorize(request) if authorize else None
      def login(self, allow_user_interaction):
        return login(allow_user_interaction) if login else False

    class MockedRequestEngine(object):
      def perform_request(self, request):
        return perform_request(request) if perform_request else None
      @classmethod
      def timeout_exception_classes(cls):
        return ()
      @classmethod
      def parse_request_exception(cls, exc):
        return None, None

    return net.HttpService(
        url,
        authenticator=MockedAuthenticator(),
        engine=MockedRequestEngine())

  def test_request_GET_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'True'

    def mock_perform_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      return request.make_fake_response(response)

    service = self.mocked_http_service(url=service_url,
        perform_request=mock_perform_request)
    self.assertEqual(service.request(request_url).read(), response)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_POST_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'True'

    def mock_perform_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      self.assertEqual('', request.body)
      return request.make_fake_response(response)

    service = self.mocked_http_service(url=service_url,
        perform_request=mock_perform_request)
    self.assertEqual(service.request(request_url, data={}).read(), response)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_PUT_success(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    request_body = 'data_body'
    response_body = 'True'
    content_type = 'application/octet-stream'

    def mock_perform_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      self.assertEqual(request_body, request.body)
      self.assertEqual(request.method, 'PUT')
      self.assertEqual(request.headers['Content-Type'], content_type)
      return request.make_fake_response(response_body)

    service = self.mocked_http_service(url=service_url,
        perform_request=mock_perform_request)
    response = service.request(request_url,
        data=request_body, content_type=content_type, method='PUT')
    self.assertEqual(response.read(), response_body)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_success_after_failure(self):
    response = 'True'
    attempts = []

    def mock_perform_request(request):
      attempts.append(request)
      if len(attempts) == 1:
        raise net.ConnectionError()
      return request.make_fake_response(response)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', data={}).read(), response)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)

  def test_request_failure_max_attempts_default(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/'), None)
    self.assertAttempts(net.URL_OPEN_MAX_ATTEMPTS, net.URL_OPEN_TIMEOUT)

  def test_request_failure_max_attempts(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', max_attempts=23), None)
    self.assertAttempts(23, net.URL_OPEN_TIMEOUT)

  def test_request_failure_timeout(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', max_attempts=10000), None)
    self.assertAttempts(int(net.URL_OPEN_TIMEOUT) + 1, net.URL_OPEN_TIMEOUT)

  def test_request_failure_timeout_default(self):
    def mock_perform_request(_request):
      raise net.ConnectionError()
    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', timeout=10.), None)
    self.assertAttempts(11, 10.0)

  def test_request_HTTP_error_no_retry(self):
    count = []
    def mock_perform_request(request):
      count.append(request)
      raise net.HttpError(400, 'text/plain', None)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertEqual(service.request('/', data={}), None)
    self.assertEqual(1, len(count))
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_request_HTTP_error_retry_404(self):
    response = 'data'
    attempts = []

    def mock_perform_request(request):
      attempts.append(request)
      if len(attempts) == 1:
        raise net.HttpError(404, 'text/plain', None)
      return request.make_fake_response(response)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    result = service.request('/', data={}, retry_404=True)
    self.assertEqual(result.read(), response)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)

  def test_request_HTTP_error_retry_404_endpoints(self):
    response = 'data'
    attempts = []

    def mock_perform_request(request):
      attempts.append(request)
      if len(attempts) == 1:
        raise net.HttpError(404, 'application/text; charset=ASCII', None)
      return request.make_fake_response(response)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    result = service.request('/_ah/api/foo/v1/bar', )
    self.assertEqual(result.read(), response)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)

  def test_request_HTTP_error_with_retry(self):
    response = 'response'
    attempts = []

    def mock_perform_request(request):
      attempts.append(request)
      if len(attempts) == 1:
        raise net.HttpError(500, 'text/plain', None)
      return request.make_fake_response(response)

    service = self.mocked_http_service(perform_request=mock_perform_request)
    self.assertTrue(service.request('/', data={}).read(), response)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)

  def test_auth_success(self):
    calls = []
    response = 'response'

    def mock_perform_request(request):
      calls.append('request')
      if 'login' not in calls:
        raise net.HttpError(403, 'text/plain', None)
      return request.make_fake_response(response)

    def mock_authorize(request):
      calls.append('authorize')

    def mock_login(allow_user_interaction):
      self.assertFalse(allow_user_interaction)
      calls.append('login')
      return True

    service = self.mocked_http_service(
        perform_request=mock_perform_request,
        authorize=mock_authorize,
        login=mock_login)
    self.assertEqual(service.request('/').read(), response)
    self.assertEqual(
        ['authorize', 'request', 'login', 'authorize', 'request'], calls)
    self.assertAttempts(2, net.URL_OPEN_TIMEOUT)
    self.assertSleeps(0)

  def test_auth_failure(self):
    count = []

    def mock_perform_request(_request):
      raise net.HttpError(403, 'text/plain', None)

    def mock_login(allow_user_interaction):
      self.assertFalse(allow_user_interaction)
      count.append(1)
      return False

    service = self.mocked_http_service(perform_request=mock_perform_request,
        login=mock_login)
    self.assertEqual(service.request('/'), None)
    self.assertEqual(len(count), 1)
    self.assertAttempts(1, net.URL_OPEN_TIMEOUT)

  def test_url_read(self):
    # Successfully reads the data.
    self.mock(net, 'url_open',
        lambda url, **_kwargs: net.HttpResponse.get_fake_response('111', url))
    self.assertEqual(net.url_read('https://fake_url.com/test'), '111')

    # Respects url_open connection errors.
    self.mock(net, 'url_open', lambda _url, **_kwargs: None)
    self.assertIsNone(net.url_read('https://fake_url.com/test'))

    # Respects read timeout errors.
    def timeouting_http_response(url):
      def read_mock(_size=None):
        raise net.TimeoutError()
      response = net.HttpResponse.get_fake_response('', url)
      self.mock(response, 'read', read_mock)
      return response

    self.mock(net, 'url_open',
        lambda url, **_kwargs: timeouting_http_response(url))
    self.assertIsNone(net.url_read('https://fake_url.com/test'))

  def test_url_retrieve(self):
    # Successfully reads the data.
    @contextlib.contextmanager
    def fake_open(_filepath, _mode):
      yield StringIO.StringIO()

    self.mock(__builtin__, 'open', fake_open)
    self.mock(net, 'url_open',
        lambda url, **_kwargs: net.HttpResponse.get_fake_response('111', url))
    self.assertEqual(
        True, net.url_retrieve('filepath', 'https://localhost/test'))

    # Respects url_open connection errors.
    self.mock(net, 'url_open', lambda _url, **_kwargs: None)
    self.assertEqual(
        False, net.url_retrieve('filepath', 'https://localhost/test'))

    # Respects read timeout errors.
    def timeouting_http_response(url):
      def read_mock(_size=None):
        raise net.TimeoutError()
      response = net.HttpResponse.get_fake_response('', url)
      self.mock(response, 'read', read_mock)
      return response

    removed = []
    self.mock(os, 'remove', removed.append)
    self.mock(net, 'url_open',
        lambda url, **_kwargs: timeouting_http_response(url))
    self.assertEqual(
        False, net.url_retrieve('filepath', 'https://localhost/test'))
    self.assertEqual(['filepath'], removed)


class TestNetFunctions(auto_stub.TestCase):
  def test_fix_url(self):
    data = [
      ('http://foo.com/', 'http://foo.com'),
      ('https://foo.com/', 'https://foo.com'),
      ('https://foo.com', 'https://foo.com'),
      ('https://foo.com/a', 'https://foo.com/a'),
      ('https://foo.com/a/', 'https://foo.com/a'),
      ('https://foo.com:8080/a/', 'https://foo.com:8080/a'),
      ('foo.com', 'https://foo.com'),
      ('foo.com:8080', 'https://foo.com:8080'),
      ('foo.com/', 'https://foo.com'),
      ('foo.com/a/', 'https://foo.com/a'),
    ]
    for value, expected in data:
      self.assertEqual(expected, net.fix_url(value))


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.FATAL))
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
