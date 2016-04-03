#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import itertools
import logging
import os
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import subprocess42


# Disable pre-set unbuffered output to not interfere with the testing being done
# here. Otherwise everything would test with unbuffered; which is fine but
# that's not what we specifically want to test here.
ENV = os.environ.copy()
ENV.pop('PYTHONUNBUFFERED', None)


SCRIPT = (
  'import signal, sys, time;\n'
  'l = [];\n'
  'def handler(signum, _):\n'
  '  l.append(signum);\n'
  '  print(\'got signal %%d\' %% signum);\n'
  '  sys.stdout.flush();\n'
  'signal.signal(%s, handler);\n'
  'print(\'hi\');\n'
  'sys.stdout.flush();\n'
  'while not l:\n'
  '  try:\n'
  '    time.sleep(0.01);\n'
  '  except IOError:\n'
  '    print(\'ioerror\');\n'
  'print(\'bye\')') % (
    'signal.SIGBREAK' if sys.platform == 'win32' else 'signal.SIGTERM')


OUTPUT_SCRIPT = r"""
import re
import sys
import time

def main():
  try:
    for command in sys.argv[1:]:
      if re.match(r'^[0-9\.]+$', command):
        time.sleep(float(command))
        continue

      if command.startswith('out_'):
        pipe = sys.stdout
      elif command.startswith('err_'):
        pipe = sys.stderr
      else:
        return 1

      command = command[4:]
      if command == 'print':
        pipe.write('printing')
      elif command == 'sleeping':
        pipe.write('Sleeping.\n')
      elif command == 'slept':
        pipe.write('Slept.\n')
      elif command == 'lf':
        pipe.write('\n')
      elif command == 'flush':
        pipe.flush()
      else:
        return 1
    return 0
  except OSError:
    return 0


if __name__ == '__main__':
  sys.exit(main())
"""


def to_native_eol(string):
  if string is None:
    return string
  if sys.platform == 'win32':
    return string.replace('\n', '\r\n')
  return string


def get_output_sleep_proc(flush, unbuffered, sleep_duration):
  """Returns process with universal_newlines=True that prints to stdout before
  after a sleep.

  It also optionally sys.stdout.flush() before the sleep and optionally enable
  unbuffered output in python.
  """
  command = [
    'import sys,time',
    'print(\'A\')',
  ]
  if flush:
    # Sadly, this doesn't work otherwise in some combination.
    command.append('sys.stdout.flush()')
  command.extend((
    'time.sleep(%s)' % sleep_duration,
    'print(\'B\')',
  ))
  cmd = [sys.executable, '-c', ';'.join(command)]
  if unbuffered:
    cmd.append('-u')
  return subprocess42.Popen(
      cmd, env=ENV, stdout=subprocess42.PIPE, universal_newlines=True)


def get_output_sleep_proc_err(sleep_duration):
  """Returns process with universal_newlines=True that prints to stderr before
  and after a sleep.
  """
  command = [
    'import sys,time',
    'sys.stderr.write(\'A\\n\')',
  ]
  command.extend((
    'time.sleep(%s)' % sleep_duration,
    'sys.stderr.write(\'B\\n\')',
  ))
  cmd = [sys.executable, '-c', ';'.join(command)]
  return subprocess42.Popen(
      cmd, env=ENV, stderr=subprocess42.PIPE, universal_newlines=True)


class Subprocess42Test(unittest.TestCase):
  def setUp(self):
    self._output_script = None
    super(Subprocess42Test, self).setUp()

  def tearDown(self):
    try:
      if self._output_script:
        os.remove(self._output_script)
    finally:
      super(Subprocess42Test, self).tearDown()

  @property
  def output_script(self):
    if not self._output_script:
      handle, self._output_script = tempfile.mkstemp(
          prefix='subprocess42', suffix='.py')
      os.write(handle, OUTPUT_SCRIPT)
      os.close(handle)
    return self._output_script

  def test_communicate_timeout(self):
    timedout = 1 if sys.platform == 'win32' else -9
    # Format is:
    # ( (cmd, stderr_pipe, timeout), (stdout, stderr, returncode) ), ...
    # See OUTPUT script for the meaning of the commands.
    test_data = [
      # 0 means no timeout, like None.
      (
        (['out_sleeping', '0.001', 'out_slept', 'err_print'], None, 0),
        ('Sleeping.\nSlept.\n', None, 0),
      ),
      (
        (['err_print'], subprocess42.STDOUT, 0),
        ('printing', None, 0),
      ),
      (
        (['err_print'], subprocess42.PIPE, 0),
        ('', 'printing', 0),
      ),

      # On a loaded system, this can be tight.
      (
        (['out_sleeping', 'out_flush', '60', 'out_slept'], None, 0.5),
        ('Sleeping.\n', None, timedout),
      ),
      (
        (
          # Note that err_flush is necessary on Windows but not on the other
          # OSes. This means the likelihood of missing stderr output from a
          # killed child process on Windows is much higher than on other OSes.
          [
            'out_sleeping', 'out_flush', 'err_print', 'err_flush', '60',
            'out_slept',
          ],
          subprocess42.PIPE,
          0.5),
        ('Sleeping.\n', 'printing', timedout),
      ),

      (
        (['out_sleeping', '0.001', 'out_slept'], None, 60),
        ('Sleeping.\nSlept.\n', None, 0),
      ),
    ]
    for i, ((args, errpipe, timeout), expected) in enumerate(test_data):
      proc = subprocess42.Popen(
          [sys.executable, self.output_script] + args,
          env=ENV,
          stdout=subprocess42.PIPE,
          stderr=errpipe)
      try:
        stdout, stderr = proc.communicate(timeout=timeout)
        code = proc.returncode
      except subprocess42.TimeoutExpired as e:
        stdout = e.output
        stderr = e.stderr
        self.assertTrue(proc.kill())
        code = proc.wait()
      finally:
        duration = proc.duration()
      expected_duration = 0.0001 if not timeout or timeout == 60 else timeout
      self.assertTrue(duration > expected_duration, (i, expected_duration))
      self.assertEqual(
          (i, stdout, stderr, code),
          (i,
            to_native_eol(expected[0]),
            to_native_eol(expected[1]),
            expected[2]))

      # Try again with universal_newlines=True.
      proc = subprocess42.Popen(
          [sys.executable, self.output_script] + args,
          env=ENV,
          stdout=subprocess42.PIPE,
          stderr=errpipe,
          universal_newlines=True)
      try:
        stdout, stderr = proc.communicate(timeout=timeout)
        code = proc.returncode
      except subprocess42.TimeoutExpired as e:
        stdout = e.output
        stderr = e.stderr
        self.assertTrue(proc.kill())
        code = proc.wait()
      finally:
        duration = proc.duration()
      self.assertTrue(duration > expected_duration, (i, expected_duration))
      self.assertEqual(
          (i, stdout, stderr, code),
          (i,) + expected)

  def test_communicate_input(self):
    cmd = [
      sys.executable, '-u', '-c',
      'import sys; sys.stdout.write(sys.stdin.read(5))',
    ]
    proc = subprocess42.Popen(
        cmd, stdin=subprocess42.PIPE, stdout=subprocess42.PIPE)
    out, err = proc.communicate(input='12345')
    self.assertEqual('12345', out)
    self.assertEqual(None, err)

  def test_communicate_input_timeout(self):
    cmd = [sys.executable, '-u', '-c', 'import time; time.sleep(60)']
    proc = subprocess42.Popen(cmd, stdin=subprocess42.PIPE)
    try:
      proc.communicate(input='12345', timeout=0.5)
      self.fail()
    except subprocess42.TimeoutExpired as e:
      self.assertEqual(None, e.output)
      self.assertEqual(None, e.stderr)
      self.assertTrue(proc.kill())
      proc.wait()
      self.assertLess(0.5, proc.duration())

  def test_communicate_input_stdout_timeout(self):
    cmd = [
      sys.executable, '-u', '-c',
      'import sys, time; sys.stdout.write(sys.stdin.read(5)); time.sleep(60)',
    ]
    proc = subprocess42.Popen(
        cmd, stdin=subprocess42.PIPE, stdout=subprocess42.PIPE)
    try:
      proc.communicate(input='12345', timeout=0.5)
      self.fail()
    except subprocess42.TimeoutExpired as e:
      self.assertEqual('12345', e.output)
      self.assertEqual(None, e.stderr)
      self.assertTrue(proc.kill())
      proc.wait()
      self.assertLess(0.5, proc.duration())

  def test_communicate_timeout_no_pipe(self):
    # In this case, it's effectively a wait() call.
    cmd = [sys.executable, '-u', '-c', 'import time; time.sleep(60)']
    proc = subprocess42.Popen(cmd)
    try:
      proc.communicate(timeout=0.5)
      self.fail()
    except subprocess42.TimeoutExpired as e:
      self.assertEqual(None, e.output)
      self.assertEqual(None, e.stderr)
      self.assertTrue(proc.kill())
      proc.wait()
      self.assertLess(0.5, proc.duration())

  def test_call(self):
    cmd = [sys.executable, '-u', '-c', 'import sys; sys.exit(0)']
    self.assertEqual(0, subprocess42.call(cmd))

    cmd = [sys.executable, '-u', '-c', 'import sys; sys.exit(1)']
    self.assertEqual(1, subprocess42.call(cmd))

  def test_check_call(self):
    cmd = [sys.executable, '-u', '-c', 'import sys; sys.exit(0)']
    self.assertEqual(0, subprocess42.check_call(cmd))

    cmd = [sys.executable, '-u', '-c', 'import sys; sys.exit(1)']
    try:
      self.assertEqual(1, subprocess42.check_call(cmd))
      self.fail()
    except subprocess42.CalledProcessError as e:
      self.assertEqual(None, e.output)

  def test_check_output(self):
    cmd = [sys.executable, '-u', '-c', 'print(\'.\')']
    self.assertEqual(
        '.\n',
        subprocess42.check_output(cmd, universal_newlines=True))

    cmd = [sys.executable, '-u', '-c', 'import sys; print(\'.\'); sys.exit(1)']
    try:
      subprocess42.check_output(cmd, universal_newlines=True)
      self.fail()
    except subprocess42.CalledProcessError as e:
      self.assertEqual('.\n', e.output)

  def test_recv_any(self):
    # Test all pipe direction and output scenarios.
    combinations = [
      {
        'cmd': ['out_print', 'err_print'],
        'stdout': None,
        'stderr': None,
        'expected': {},
      },
      {
        'cmd': ['out_print', 'err_print'],
        'stdout': None,
        'stderr': subprocess42.STDOUT,
        'expected': {},
      },

      {
        'cmd': ['out_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.PIPE,
        'expected': {'stdout': 'printing'},
      },
      {
        'cmd': ['out_print'],
        'stdout': subprocess42.PIPE,
        'stderr': None,
        'expected': {'stdout': 'printing'},
      },
      {
        'cmd': ['out_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.STDOUT,
        'expected': {'stdout': 'printing'},
      },

      {
        'cmd': ['err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.PIPE,
        'expected': {'stderr': 'printing'},
      },
      {
        'cmd': ['err_print'],
        'stdout': None,
        'stderr': subprocess42.PIPE,
        'expected': {'stderr': 'printing'},
      },
      {
        'cmd': ['err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.STDOUT,
        'expected': {'stdout': 'printing'},
      },

      {
        'cmd': ['out_print', 'err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.PIPE,
        'expected': {'stderr': 'printing', 'stdout': 'printing'},
      },
      {
        'cmd': ['out_print', 'err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.STDOUT,
        'expected': {'stdout': 'printingprinting'},
      },
    ]
    for i, testcase in enumerate(combinations):
      cmd = [sys.executable, self.output_script] + testcase['cmd']
      p = subprocess42.Popen(
          cmd, env=ENV, stdout=testcase['stdout'], stderr=testcase['stderr'])
      actual = {}
      while p.poll() is None:
        pipe, data = p.recv_any()
        if data:
          actual.setdefault(pipe, '')
          actual[pipe] += data

      # The process exited, read any remaining data in the pipes.
      while True:
        pipe, data = p.recv_any()
        if pipe is None:
          break
        actual.setdefault(pipe, '')
        actual[pipe] += data
      self.assertEqual(
          testcase['expected'],
          actual,
          (i, testcase['cmd'], testcase['expected'], actual))
      self.assertEqual((None, None), p.recv_any())
      self.assertEqual(0, p.returncode)

  def test_recv_any_different_buffering(self):
    # Specifically test all buffering scenarios.
    for flush, unbuffered in itertools.product([True, False], [True, False]):
      actual = ''
      proc = get_output_sleep_proc(flush, unbuffered, 0.5)
      while True:
        p, data = proc.recv_any()
        if not p:
          break
        self.assertEqual('stdout', p)
        self.assertTrue(data, (p, data))
        actual += data

      self.assertEqual('A\nB\n', actual)
      # Contrary to yield_any() or recv_any(0), wait() needs to be used here.
      proc.wait()
      self.assertEqual(0, proc.returncode)

  def test_recv_any_timeout_0(self):
    # rec_any() is expected to timeout and return None with no data pending at
    # least once, due to the sleep of 'duration' and the use of timeout=0.
    for flush, unbuffered in itertools.product([True, False], [True, False]):
      for duration in (0.05, 0.1, 0.5, 2):
        try:
          actual = ''
          proc = get_output_sleep_proc(flush, unbuffered, duration)
          try:
            got_none = False
            while True:
              p, data = proc.recv_any(timeout=0)
              if not p:
                if proc.poll() is None:
                  got_none = True
                  continue
                break
              self.assertEqual('stdout', p)
              self.assertTrue(data, (p, data))
              actual += data

            self.assertEqual('A\nB\n', actual)
            self.assertEqual(0, proc.returncode)
            self.assertEqual(True, got_none)
            break
          finally:
            proc.kill()
            proc.wait()
        except AssertionError:
          if duration != 2:
            print('Sleeping rocks. Trying slower.')
            continue
          raise

  def _test_recv_common(self, proc, is_err):
    actual = ''
    while True:
      if is_err:
        data = proc.recv_err()
      else:
        data = proc.recv_out()
      if not data:
        break
      self.assertTrue(data)
      actual += data

    self.assertEqual('A\nB\n', actual)
    proc.wait()
    self.assertEqual(0, proc.returncode)

  def test_yield_any_no_timeout(self):
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = get_output_sleep_proc(True, True, duration)
        try:
          expected = [
            'A\n',
            'B\n',
          ]
          for p, data in proc.yield_any():
            self.assertEqual('stdout', p)
            self.assertEqual(expected.pop(0), data)
          self.assertEqual(0, proc.returncode)
          self.assertEqual([], expected)
          break
        finally:
          proc.kill()
          proc.wait()
      except AssertionError:
        if duration != 2:
          print('Sleeping rocks. Trying slower.')
          continue
        raise

  def test_yield_any_timeout_0(self):
    # rec_any() is expected to timeout and return None with no data pending at
    # least once, due to the sleep of 'duration' and the use of timeout=0.
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = get_output_sleep_proc(True, True, duration)
        try:
          expected = [
            'A\n',
            'B\n',
          ]
          got_none = False
          for p, data in proc.yield_any(timeout=0):
            if not p:
              got_none = True
              continue
            self.assertEqual('stdout', p)
            self.assertEqual(expected.pop(0), data)
          self.assertEqual(0, proc.returncode)
          self.assertEqual([], expected)
          self.assertEqual(True, got_none)
          break
        finally:
          proc.kill()
          proc.wait()
      except AssertionError:
        if duration != 2:
          print('Sleeping rocks. Trying slower.')
          continue
        raise

  def test_yield_any_timeout_0_called(self):
    # rec_any() is expected to timeout and return None with no data pending at
    # least once, due to the sleep of 'duration' and the use of timeout=0.
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = get_output_sleep_proc(True, True, duration)
        try:
          expected = [
            'A\n',
            'B\n',
          ]
          got_none = False
          called = []
          def timeout():
            called.append(0)
            return 0
          for p, data in proc.yield_any(timeout=timeout):
            if not p:
              got_none = True
              continue
            self.assertEqual('stdout', p)
            self.assertEqual(expected.pop(0), data)
          self.assertEqual(0, proc.returncode)
          self.assertEqual([], expected)
          self.assertEqual(True, got_none)
          self.assertTrue(called)
          break
        finally:
          proc.kill()
          proc.wait()
      except AssertionError:
        if duration != 2:
          print('Sleeping rocks. Trying slower.')
          continue
        raise

  def test_yield_any_returncode(self):
    proc = subprocess42.Popen(
        [sys.executable, '-c', 'import sys;sys.stdout.write("yo");sys.exit(1)'],
        stdout=subprocess42.PIPE)
    for p, d in proc.yield_any():
      self.assertEqual('stdout', p)
      self.assertEqual('yo', d)
    # There was a bug where the second call to wait() would overwrite
    # proc.returncode with 0 when timeout is not None.
    self.assertEqual(1, proc.wait())
    self.assertEqual(1, proc.wait(timeout=0))
    self.assertEqual(1, proc.poll())
    self.assertEqual(1, proc.returncode)
    # On Windows, the clock resolution is 15ms so Popen.duration() will likely
    # be 0.
    self.assertLessEqual(0, proc.duration())

  def test_detached(self):
    is_win = (sys.platform == 'win32')
    cmd = [sys.executable, '-c', SCRIPT]
    # TODO(maruel): Make universal_newlines=True work and not hang.
    proc = subprocess42.Popen(cmd, detached=True, stdout=subprocess42.PIPE)
    try:
      if is_win:
        # There's something extremely weird happening on the Swarming bots in
        # the sense that they return 'hi\n' on Windows, while running the script
        # locally on Windows results in 'hi\r\n'. Author raises fist.
        self.assertIn(proc.recv_out(), ('hi\r\n', 'hi\n'))
      else:
        self.assertEqual('hi\n', proc.recv_out())

      proc.terminate()

      if is_win:
        # What happens on Windows is that the process is immediately killed
        # after handling SIGBREAK.
        self.assertEqual(0, proc.wait())
        # Windows...
        self.assertIn(
            proc.recv_any(),
            (
              ('stdout', 'got signal 21\r\nioerror\r\nbye\r\n'),
              ('stdout', 'got signal 21\nioerror\nbye\n'),
              ('stdout', 'got signal 21\r\nbye\r\n'),
              ('stdout', 'got signal 21\nbye\n'),
            ))
      else:
        self.assertEqual(0, proc.wait())
        self.assertEqual(('stdout', 'got signal 15\nbye\n'), proc.recv_any())
    finally:
      # In case the test fails.
      proc.kill()
      proc.wait()

  def test_attached(self):
    is_win = (sys.platform == 'win32')
    cmd = [sys.executable, '-c', SCRIPT]
    # TODO(maruel): Make universal_newlines=True work and not hang.
    proc = subprocess42.Popen(cmd, detached=False, stdout=subprocess42.PIPE)
    try:
      if is_win:
        # There's something extremely weird happening on the Swarming bots in
        # the sense that they return 'hi\n' on Windows, while running the script
        # locally on Windows results in 'hi\r\n'. Author raises fist.
        self.assertIn(proc.recv_out(), ('hi\r\n', 'hi\n'))
      else:
        self.assertEqual('hi\n', proc.recv_out())

      proc.terminate()

      if is_win:
        # If attached, it's hard killed.
        self.assertEqual(1, proc.wait())
        self.assertEqual((None, None), proc.recv_any())
      else:
        self.assertEqual(0, proc.wait())
        self.assertEqual(('stdout', 'got signal 15\nbye\n'), proc.recv_any())
    finally:
      # In case the test fails.
      proc.kill()
      proc.wait()


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
