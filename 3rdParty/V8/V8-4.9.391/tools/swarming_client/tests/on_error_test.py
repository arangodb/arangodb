#!/usr/bin/env python
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import BaseHTTPServer
import atexit
import cgi
import getpass
import json
import logging
import os
import platform
import re
import socket
import ssl
import subprocess
import sys
import threading
import unittest
import urllib
import urlparse

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(TESTS_DIR)
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from utils import on_error


PEM = os.path.join(TESTS_DIR, 'self_signed.pem')


# Access to a protected member XXX of a client class - pylint: disable=W0212


def _serialize_env():
  return dict(
      (unicode(k), unicode(v.encode('ascii', 'replace')))
      for k, v in os.environ.iteritems())


class HttpsServer(BaseHTTPServer.HTTPServer):
  def __init__(self, addr, cls, hostname, pem):
    BaseHTTPServer.HTTPServer.__init__(self, addr, cls)
    self.hostname = hostname
    self.pem = pem
    self.socket = ssl.wrap_socket(
        self.socket,
        server_side=True,
        certfile=self.pem)
    self.keep_running = True
    self.requests = []
    self._thread = None

  @property
  def url(self):
    return 'https://%s:%d' % (self.hostname, self.server_address[1])

  def start(self):
    assert not self._thread

    def _server_loop():
      while self.keep_running:
        self.handle_request()

    self._thread = threading.Thread(name='http', target=_server_loop)
    self._thread.daemon = True
    self._thread.start()

    while True:
      # Ensures it is up.
      try:
        urllib.urlopen(self.url + '/_warmup').read()
      except IOError:
        continue
      return

  def stop(self):
    self.keep_running = False
    urllib.urlopen(self.url + '/_quit').read()
    self._thread.join()
    self._thread = None

  def register_call(self, request):
    if request.path not in ('/_quit', '/_warmup'):
      self.requests.append((request.path, request.parse_POST()))


class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
  def log_message(self, fmt, *args):
    logging.debug(
        '%s - - [%s] %s',
        self.address_string(), self.log_date_time_string(), fmt % args)

  def parse_POST(self):
    ctype, pdict = cgi.parse_header(self.headers['Content-Type'])
    if ctype == 'multipart/form-data':
      return cgi.parse_multipart(self.rfile, pdict)
    if ctype == 'application/x-www-form-urlencoded':
      length = int(self.headers['Content-Length'])
      return urlparse.parse_qs(self.rfile.read(length), keep_blank_values=1)
    if ctype in ('application/json', 'application/json; charset=utf-8'):
      length = int(self.headers['Content-Length'])
      return json.loads(self.rfile.read(length))
    assert False, ctype

  def do_GET(self):
    self.server.register_call(self)
    self.send_response(200)
    self.send_header('Content-type', 'text/plain')
    self.end_headers()
    self.wfile.write('Rock on')

  def do_POST(self):
    self.server.register_call(self)
    self.send_response(200)
    self.send_header('Content-type', 'application/json; charset=utf-8')
    self.end_headers()
    data = {
      'id': '1234',
      'url': 'https://localhost/error/1234',
    }
    self.wfile.write(json.dumps(data))


def start_server():
  """Starts an HTTPS web server and returns the port bound."""
  # A premade passwordless self-signed certificate. It works because urllib
  # doesn't verify the certificate validity.
  httpd = HttpsServer(('127.0.0.1', 0), Handler, 'localhost', pem=PEM)
  httpd.start()
  return httpd


class OnErrorBase(auto_stub.TestCase):
  HOSTNAME = socket.getfqdn()

  def setUp(self):
    super(OnErrorBase, self).setUp()
    os.chdir(TESTS_DIR)
    self._atexit = []
    self.mock(atexit, 'register', self._atexit.append)
    self.mock(on_error, '_ENABLED_DOMAINS', (self.HOSTNAME,))
    self.mock(on_error, '_HOSTNAME', None)
    self.mock(on_error, '_SERVER', None)
    self.mock(on_error, '_is_in_test', lambda: False)


class OnErrorTest(OnErrorBase):
  def test_report(self):
    url = 'https://localhost/'
    on_error.report_on_exception_exit(url)
    self.assertEqual([on_error._check_for_exception_on_exit], self._atexit)
    self.assertEqual('https://localhost', on_error._SERVER.urlhost)
    self.assertEqual(self.HOSTNAME, on_error._HOSTNAME)
    with self.assertRaises(ValueError):
      on_error.report_on_exception_exit(url)

  def test_no_http(self):
    # http:// url are denied.
    url = 'http://localhost/'
    self.assertIs(False, on_error.report_on_exception_exit(url))
    self.assertEqual([], self._atexit)


class OnErrorServerTest(OnErrorBase):
  def call(self, url, arg, returncode):
    cmd = [sys.executable, 'on_error_test.py', 'run_shell_out', url, arg]
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=os.environ,
        universal_newlines=True)
    out = proc.communicate()[0]
    logging.debug('\n%s', out)
    self.assertEqual(returncode, proc.returncode)
    return out

  def one_request(self, httpd):
    self.assertEqual(1, len(httpd.requests))
    resource, params = httpd.requests[0]
    self.assertEqual('/ereporter2/api/v1/on_error', resource)
    self.assertEqual(['r', 'v'], params.keys())
    self.assertEqual('1', params['v'])
    return params['r']

  def test_shell_out_hacked(self):
    # Rerun itself, report an error, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'hacked', 0)
    self.assertEqual([], httpd.requests)
    self.assertEqual('', out)
    httpd.stop()

  def test_shell_out_report(self):
    # Rerun itself, report an error manually, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'report', 0)
    expected = (
        'Sending the report ... done.\n'
        'Report URL: https://localhost/error/1234\n'
        'Oh dang\n')
    self.assertEqual(expected, out)

    actual = self.one_request(httpd)
    self.assertGreater(actual.pop('duration'), 0.000001)
    expected = {
      u'args': [
        u'on_error_test.py', u'run_shell_out', unicode(httpd.url), u'report',
      ],
      u'category': u'report',
      u'cwd': unicode(os.getcwd()),
      u'env': _serialize_env(),
      u'hostname': unicode(socket.getfqdn()),
      u'message': u'Oh dang',
      u'os': unicode(sys.platform),
      u'python_version': unicode(platform.python_version()),
      u'source': u'on_error_test.py',
      u'user': unicode(getpass.getuser()),
      # The version was added dynamically for testing purpose.
      u'version': u'123',
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_exception(self):
    # Rerun itself, report an exception manually, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'exception', 0)
    expected = (
        'Sending the crash report ... done.\n'
        'Report URL: https://localhost/error/1234\n'
        'Really\nYou are not my type\n')
    self.assertEqual(expected, out)

    actual = self.one_request(httpd)
    self.assertGreater(actual.pop('duration'), 0.000001)
    # Remove numbers so editing the code doesn't invalidate the expectation.
    actual['stack'] = re.sub(r' \d+', ' 0', actual['stack'])
    expected = {
      u'args': [
        u'on_error_test.py', u'run_shell_out', unicode(httpd.url), u'exception',
      ],
      u'cwd': unicode(os.getcwd()),
      u'category': u'exception',
      u'env': _serialize_env(),
      u'exception_type': u'TypeError',
      u'hostname': unicode(socket.getfqdn()),
      u'message': u'Really\nYou are not my type',
      u'os': unicode(sys.platform),
      u'python_version': unicode(platform.python_version()),
      u'source': u'on_error_test.py',
      u'stack':
        u'File "on_error_test.py", line 0, in run_shell_out\n'
        u'  raise TypeError(\'You are not my type\')',
      u'user': unicode(getpass.getuser()),
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_exception_no_msg(self):
    # Rerun itself, report an exception manually, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'exception_no_msg', 0)
    expected = (
        'Sending the crash report ... done.\n'
        'Report URL: https://localhost/error/1234\n'
        'You are not my type #2\n')
    self.assertEqual(expected, out)

    actual = self.one_request(httpd)
    self.assertGreater(actual.pop('duration'), 0.000001)
    # Remove numbers so editing the code doesn't invalidate the expectation.
    actual['stack'] = re.sub(r' \d+', ' 0', actual['stack'])
    expected = {
      u'args': [
        u'on_error_test.py', u'run_shell_out', unicode(httpd.url),
        u'exception_no_msg',
      ],
      u'category': u'exception',
      u'cwd': unicode(os.getcwd()),
      u'env': _serialize_env(),
      u'exception_type': u'TypeError',
      u'hostname': unicode(socket.getfqdn()),
      u'message': u'You are not my type #2',
      u'os': unicode(sys.platform),
      u'python_version': unicode(platform.python_version()),
      u'source': u'on_error_test.py',
      u'stack':
        u'File "on_error_test.py", line 0, in run_shell_out\n'
        u'  raise TypeError(\'You are not my type #2\')',
      u'user': unicode(getpass.getuser()),
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_crash(self):
    # Rerun itself, report an error with a crash, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'crash', 1)
    expected = (
        'Traceback (most recent call last):\n'
        '  File "on_error_test.py", line 0, in <module>\n'
        '    sys.exit(run_shell_out(sys.argv[2], sys.argv[3]))\n'
        '  File "on_error_test.py", line 0, in run_shell_out\n'
        '    raise ValueError(\'Oops\')\n'
        'ValueError: Oops\n'
        'Sending the crash report ... done.\n'
        'Report URL: https://localhost/error/1234\n'
        'Process exited due to exception\n'
        'Oops\n')
    # Remove numbers so editing the code doesn't invalidate the expectation.
    self.assertEqual(expected, re.sub(r' \d+', ' 0', out))

    actual = self.one_request(httpd)
    # Remove numbers so editing the code doesn't invalidate the expectation.
    actual['stack'] = re.sub(r' \d+', ' 0', actual['stack'])
    self.assertGreater(actual.pop('duration'), 0.000001)
    expected = {
      u'args': [
        u'on_error_test.py', u'run_shell_out', unicode(httpd.url), u'crash',
      ],
      u'category': u'exception',
      u'cwd': unicode(os.getcwd()),
      u'env': _serialize_env(),
      u'exception_type': u'ValueError',
      u'hostname': unicode(socket.getfqdn()),
      u'message': u'Process exited due to exception\nOops',
      u'os': unicode(sys.platform),
      u'python_version': unicode(platform.python_version()),
      u'source': u'on_error_test.py',
      # The stack trace is stripped off the heading and absolute paths.
      u'stack':
        u'File "on_error_test.py", line 0, in <module>\n'
        u'  sys.exit(run_shell_out(sys.argv[2], sys.argv[3]))\n'
        u'File "on_error_test.py", line 0, in run_shell_out\n'
        u'  raise ValueError(\'Oops\')',
      u'user': unicode(getpass.getuser()),
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_crash_server_down(self):
    # Rerun itself, report an error, ensure the error was reported.
    out = self.call('https://localhost:1', 'crash', 1)
    expected = (
        'Traceback (most recent call last):\n'
        '  File "on_error_test.py", line 0, in <module>\n'
        '    sys.exit(run_shell_out(sys.argv[2], sys.argv[3]))\n'
        '  File "on_error_test.py", line 0, in run_shell_out\n'
        '    raise ValueError(\'Oops\')\n'
        'ValueError: Oops\n'
        'Sending the crash report ... failed!\n'
        'Process exited due to exception\n'
        'Oops\n')
    # Remove numbers so editing the code doesn't invalidate the expectation.
    self.assertEqual(expected, re.sub(r' \d+', ' 0', out))


def run_shell_out(url, mode):
  # Enable 'report_on_exception_exit' even though main file is *_test.py.
  on_error._is_in_test = lambda: False

  # Hack it out so registering works.
  on_error._ENABLED_DOMAINS = (socket.getfqdn(),)

  # Don't try to authenticate into localhost.
  on_error.net.OAuthAuthenticator = lambda *_: None

  if not on_error.report_on_exception_exit(url):
    print 'Failure to register the handler'
    return 1

  # Hack out certificate verification because we are using a self-signed
  # certificate here. In practice, the SSL certificate is signed to guard
  # against MITM attacks.
  on_error._SERVER.engine.session.verify = False

  if mode == 'crash':
    # Sadly, net is a bit overly verbose, which breaks
    # test_shell_out_crash_server_down.
    logging.error = lambda *_, **_kwargs: None
    logging.warning = lambda *_, **_kwargs: None
    raise ValueError('Oops')

  if mode == 'report':
    # Generate a manual report without an exception frame. Also set the version
    # value.
    setattr(sys.modules['__main__'], '__version__', '123')
    on_error.report('Oh dang')

  if mode == 'exception':
    # Report from inside an exception frame.
    try:
      raise TypeError('You are not my type')
    except TypeError:
      on_error.report('Really')

  if mode == 'exception_no_msg':
    # Report from inside an exception frame.
    try:
      raise TypeError('You are not my type #2')
    except TypeError:
      on_error.report(None)
  return 0


if __name__ == '__main__':
  # Ignore _DISABLE_ENVVAR if set.
  os.environ.pop(on_error._DISABLE_ENVVAR, None)

  if len(sys.argv) == 4 and sys.argv[1] == 'run_shell_out':
    sys.exit(run_shell_out(sys.argv[2], sys.argv[3]))

  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
