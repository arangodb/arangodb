#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import BaseHTTPServer
import logging
import os
import re
import SocketServer
import sys
import threading
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from utils import net


class SleepingServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
  """Multithreaded server that serves requests that block at various stages."""

  # Lingering keep-alive HTTP connections keep (not very smart) HTTPServer
  # threads alive as well. Convert them to deamon threads so that they don't
  # block process exit.
  daemon_threads = True

  def __init__(self):
    BaseHTTPServer.HTTPServer.__init__(self, ('127.0.0.1', 0), SleepingHandler)
    self.dying = False
    self.dying_cv = threading.Condition()
    self.serving_thread = None

  def handle_error(self, _request, _client_address):
    # Mute "error: [Errno 32] Broken pipe" errors.
    pass

  def start(self):
    self.serving_thread = threading.Thread(target=self.serve_forever,
                                           kwargs={'poll_interval': 0.05})
    self.serving_thread.start()

  def stop(self):
    with self.dying_cv:
      self.dying = True
      self.dying_cv.notifyAll()
    self.shutdown()

  @property
  def url(self):
    return 'http://%s:%d' % self.socket.getsockname()

  def sleep(self, timeout):
    deadline = time.time() + timeout
    with self.dying_cv:
      while not self.dying and time.time() < deadline:
        self.dying_cv.wait(deadline - time.time())


class SleepingHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  protocol_version = 'HTTP/1.1'

  path_re = re.compile(r'/(.*)/([\.\d]*)(\?.*)?')

  first_line = 'FIRST LINE\n'
  second_line = 'SECOND LINE\n'
  full_response = first_line + second_line

  modes = {
    'sleep_before_response': ['SLEEP', 'HEADERS', 'FIRST', 'SECOND'],
    'sleep_after_headers': ['HEADERS', 'SLEEP', 'FIRST', 'SECOND'],
    'sleep_during_response': ['HEADERS', 'FIRST', 'SLEEP', 'SECOND'],
    'sleep_after_response': ['HEADERS', 'FIRST', 'SECOND', 'SLEEP'],
  }

  def send_headers(self):
    self.send_response(200)
    self.send_header('Content-Length', len(self.full_response))
    self.end_headers()

  def log_message(self, _format, *_args):
    # Mute "GET /sleep_before_response/0.000000 HTTP/1.1" 200 -" messages.
    pass

  def do_GET(self):
    # Split request string like '/sleep/0.1?param=1' into ('sleep', 0.1) pair.
    match = self.path_re.match(self.path)
    if not match:
      self.send_error(404)
      return
    mode, timeout, _ = match.groups()
    # Ensure timeout is float.
    try:
      timeout = float(timeout)
    except ValueError:
      self.send_error(400)
      return
    # Ensure mode is known.
    if mode not in self.modes:
      self.send_error(404)
      return
    # Mapping mode's action -> function to call.
    actions = {
      'SLEEP': lambda: self.server.sleep(timeout),
      'HEADERS': self.send_headers,
      'FIRST': lambda: self.wfile.write(self.first_line),
      'SECOND': lambda: self.wfile.write(self.second_line),
    }
    # Execute all actions defined by the mode.
    for action in self.modes[mode]:
      actions[action]()


class UrlOpenTimeoutTest(auto_stub.TestCase):
  def setUp(self):
    super(UrlOpenTimeoutTest, self).setUp()
    self.mock(net, 'OAuthAuthenticator', lambda *_: None)
    self.server = SleepingServer()
    self.server.start()

  def tearDown(self):
    self.server.stop()
    self.server = None
    super(UrlOpenTimeoutTest, self).tearDown()

  def call(self, mode, sleep_duration, **kwargs):
    url = self.server.url + '/%s/%f' % (mode, sleep_duration)
    kwargs['max_attempts'] = 2
    return net.url_open(url, **kwargs)

  def test_urlopen_success(self):
    # Server doesn't block.
    for mode in SleepingHandler.modes:
      self.assertEqual(self.call(mode, 0, read_timeout=0.1).read(),
                       SleepingHandler.full_response)
    # Server does block, but url_open called without read timeout.
    for mode in SleepingHandler.modes:
      self.assertEqual(self.call(mode, 0.25, read_timeout=None).read(),
                       SleepingHandler.full_response)

  def test_urlopen_retry(self):
    # This should trigger retry logic and eventually return None.
    self.mock(net, 'sleep_before_retry', lambda *_: None)
    stream = self.call('sleep_before_response', 0.25, read_timeout=0.1)
    self.assertIsNone(stream)

  def test_urlopen_keeping_connection(self):
    # Sleeping after request is sent -> it's just connection keep alive.
    stream = self.call('sleep_after_response', 0.25, read_timeout=0.1)
    self.assertEqual(stream.read(), SleepingHandler.full_response)

  def test_urlopen_timeouts(self):
    # Timeouts while reading from the stream.
    for mode in ('sleep_after_headers', 'sleep_during_response'):
      stream = self.call(mode, 0.25, read_timeout=0.1)
      self.assertTrue(stream)
      with self.assertRaises(net.TimeoutError):
        stream.read()


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
