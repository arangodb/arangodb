#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# Lambda may not be necessary.
# pylint: disable=W0108

import functools
import logging
import os
import signal
import subprocess
import sys
import threading
import time
import traceback
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import threading_utils


def timeout(max_running_time):
  """Test method decorator that fails the test if it executes longer
  than |max_running_time| seconds.

  It exists to terminate tests in case of deadlocks. There's a high chance that
  process is broken after such timeout (due to hanging deadlocked threads that
  can own some shared resources). But failing early (maybe not in a cleanest
  way) due to timeout is generally better than hanging indefinitely.

  |max_running_time| should be an order of magnitude (or even two orders) larger
  than the expected run time of the test to compensate for slow machine, high
  CPU utilization by some other processes, etc.

  Can not be nested.

  Noop on windows (since win32 doesn't support signal.setitimer).
  """
  if sys.platform == 'win32':
    return lambda method: method

  def decorator(method):
    @functools.wraps(method)
    def wrapper(self, *args, **kwargs):
      signal.signal(signal.SIGALRM, lambda *_args: self.fail('Timeout'))
      signal.setitimer(signal.ITIMER_REAL, max_running_time)
      try:
        return method(self, *args, **kwargs)
      finally:
        signal.signal(signal.SIGALRM, signal.SIG_DFL)
        signal.setitimer(signal.ITIMER_REAL, 0)
    return wrapper

  return decorator


class ThreadPoolTest(unittest.TestCase):
  MIN_THREADS = 0
  MAX_THREADS = 32

  # Append custom assert messages to default ones (works with python >= 2.7).
  longMessage = True

  @staticmethod
  def sleep_task(duration=0.01):
    """Returns function that sleeps |duration| sec and returns its argument."""
    def task(arg):
      time.sleep(duration)
      return arg
    return task

  def retrying_sleep_task(self, duration=0.01):
    """Returns function that adds sleep_task to the thread pool."""
    def task(arg):
      self.thread_pool.add_task(0, self.sleep_task(duration), arg)
    return task

  @staticmethod
  def none_task():
    """Returns function that returns None."""
    return lambda _arg: None

  def setUp(self):
    super(ThreadPoolTest, self).setUp()
    self.thread_pool = threading_utils.ThreadPool(
        self.MIN_THREADS, self.MAX_THREADS, 0)

  @timeout(1)
  def tearDown(self):
    super(ThreadPoolTest, self).tearDown()
    self.thread_pool.close()

  def get_results_via_join(self, _expected):
    return self.thread_pool.join()

  def get_results_via_get_one_result(self, expected):
    return [self.thread_pool.get_one_result() for _ in expected]

  def get_results_via_iter_results(self, _expected):
    return list(self.thread_pool.iter_results())

  def run_results_test(self, task, results_getter, args=None, expected=None):
    """Template function for tests checking that pool returns all results.

    Will add multiple instances of |task| to the thread pool, then call
    |results_getter| to get back all results and compare them to expected ones.
    """
    args = range(0, 100) if args is None else args
    expected = args if expected is None else expected
    msg = 'Using \'%s\' to get results.' % (results_getter.__name__,)

    for i in args:
      self.thread_pool.add_task(0, task, i)
    results = results_getter(expected)

    # Check that got all results back (exact same set, no duplicates).
    self.assertEqual(set(expected), set(results), msg)
    self.assertEqual(len(expected), len(results), msg)

    # Queue is empty, result request should fail.
    with self.assertRaises(threading_utils.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(1)
  def test_get_one_result_ok(self):
    self.thread_pool.add_task(0, lambda: 'OK')
    self.assertEqual(self.thread_pool.get_one_result(), 'OK')

  @timeout(1)
  def test_get_one_result_fail(self):
    # No tasks added -> get_one_result raises an exception.
    with self.assertRaises(threading_utils.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(5)
  def test_join(self):
    self.run_results_test(self.sleep_task(),
                          self.get_results_via_join)

  @timeout(5)
  def test_get_one_result(self):
    self.run_results_test(self.sleep_task(),
                          self.get_results_via_get_one_result)

  @timeout(5)
  def test_iter_results(self):
    self.run_results_test(self.sleep_task(),
                          self.get_results_via_iter_results)

  @timeout(5)
  def test_retry_and_join(self):
    self.run_results_test(self.retrying_sleep_task(),
                          self.get_results_via_join)

  @timeout(5)
  def test_retry_and_get_one_result(self):
    self.run_results_test(self.retrying_sleep_task(),
                          self.get_results_via_get_one_result)

  @timeout(5)
  def test_retry_and_iter_results(self):
    self.run_results_test(self.retrying_sleep_task(),
                          self.get_results_via_iter_results)

  @timeout(5)
  def test_none_task_and_join(self):
    self.run_results_test(self.none_task(),
                          self.get_results_via_join,
                          expected=[])

  @timeout(5)
  def test_none_task_and_get_one_result(self):
    self.thread_pool.add_task(0, self.none_task(), 0)
    with self.assertRaises(threading_utils.ThreadPoolEmpty):
      self.thread_pool.get_one_result()

  @timeout(5)
  def test_none_task_and_and_iter_results(self):
    self.run_results_test(self.none_task(),
                          self.get_results_via_iter_results,
                          expected=[])

  @timeout(5)
  def test_generator_task(self):
    MULTIPLIER = 1000
    COUNT = 10

    # Generator that yields [i * MULTIPLIER, i * MULTIPLIER + COUNT).
    def generator_task(i):
      for j in xrange(COUNT):
        time.sleep(0.001)
        yield i * MULTIPLIER + j

    # Arguments for tasks and expected results.
    args = range(0, 10)
    expected = [i * MULTIPLIER + j for i in args for j in xrange(COUNT)]

    # Test all possible ways to pull results from the thread pool.
    getters = (self.get_results_via_join,
               self.get_results_via_iter_results,
               self.get_results_via_get_one_result,)
    for results_getter in getters:
      self.run_results_test(generator_task, results_getter, args, expected)

  @timeout(5)
  def test_concurrent_iter_results(self):
    def poller_proc(result):
      result.extend(self.thread_pool.iter_results())

    args = range(0, 100)
    for i in args:
      self.thread_pool.add_task(0, self.sleep_task(), i)

    # Start a bunch of threads, all calling iter_results in parallel.
    pollers = []
    for _ in xrange(0, 4):
      result = []
      poller = threading.Thread(target=poller_proc, args=(result,))
      poller.start()
      pollers.append((poller, result))

    # Collects results from all polling threads.
    all_results = []
    for poller, results in pollers:
      poller.join()
      all_results.extend(results)

    # Check that got all results back (exact same set, no duplicates).
    self.assertEqual(set(args), set(all_results))
    self.assertEqual(len(args), len(all_results))

  @timeout(1)
  def test_adding_tasks_after_close(self):
    pool = threading_utils.ThreadPool(1, 1, 0)
    pool.add_task(0, lambda: None)
    pool.close()
    with self.assertRaises(threading_utils.ThreadPoolClosed):
      pool.add_task(0, lambda: None)

  @timeout(1)
  def test_double_close(self):
    pool = threading_utils.ThreadPool(1, 1, 0)
    pool.close()
    with self.assertRaises(threading_utils.ThreadPoolClosed):
      pool.close()

  def test_priority(self):
    # Verifies that a lower priority is run first.
    with threading_utils.ThreadPool(1, 1, 0) as pool:
      lock = threading.Lock()

      def wait_and_return(x):
        with lock:
          return x

      def return_x(x):
        return x

      with lock:
        pool.add_task(0, wait_and_return, 'a')
        pool.add_task(2, return_x, 'b')
        pool.add_task(1, return_x, 'c')

      actual = pool.join()
    self.assertEqual(['a', 'c', 'b'], actual)

  @timeout(2)
  def test_abort(self):
    # Trigger a ridiculous amount of tasks, and abort the remaining.
    with threading_utils.ThreadPool(2, 2, 0) as pool:
      # Allow 10 tasks to run initially.
      sem = threading.Semaphore(10)

      def grab_and_return(x):
        sem.acquire()
        return x

      for i in range(100):
        pool.add_task(0, grab_and_return, i)

      # Running at 11 would hang.
      results = [pool.get_one_result() for _ in xrange(10)]
      # At that point, there's 10 completed tasks and 2 tasks hanging, 88
      # pending.
      self.assertEqual(88, pool.abort())
      # Calling .join() before these 2 .release() would hang.
      sem.release()
      sem.release()
      results.extend(pool.join())
    # The results *may* be out of order. Even if the calls are processed
    # strictly in FIFO mode, a thread may preempt another one when returning the
    # values.
    self.assertEqual(range(12), sorted(results))


class AutoRetryThreadPoolTest(unittest.TestCase):
  def test_bad_class(self):
    exceptions = [AutoRetryThreadPoolTest]
    with self.assertRaises(AssertionError):
      threading_utils.AutoRetryThreadPool(exceptions, 1, 0, 1, 0)

  def test_no_exception(self):
    with self.assertRaises(AssertionError):
      threading_utils.AutoRetryThreadPool([], 1, 0, 1, 0)

  def test_bad_retry(self):
    exceptions = [IOError]
    with self.assertRaises(AssertionError):
      threading_utils.AutoRetryThreadPool(exceptions, 256, 0, 1, 0)

  def test_bad_priority(self):
    exceptions = [IOError]
    with threading_utils.AutoRetryThreadPool(exceptions, 1, 1, 1, 0) as pool:
      pool.add_task(0, lambda x: x, 0)
      pool.add_task(256, lambda x: x, 0)
      pool.add_task(512, lambda x: x, 0)
      with self.assertRaises(AssertionError):
        pool.add_task(1, lambda x: x, 0)
      with self.assertRaises(AssertionError):
        pool.add_task(255, lambda x: x, 0)

  def test_priority(self):
    # Verifies that a lower priority is run first.
    exceptions = [IOError]
    with threading_utils.AutoRetryThreadPool(exceptions, 1, 1, 1, 0) as pool:
      lock = threading.Lock()

      def wait_and_return(x):
        with lock:
          return x

      def return_x(x):
        return x

      with lock:
        pool.add_task(threading_utils.PRIORITY_HIGH, wait_and_return, 'a')
        pool.add_task(threading_utils.PRIORITY_LOW, return_x, 'b')
        pool.add_task(threading_utils.PRIORITY_MED, return_x, 'c')

      actual = pool.join()
    self.assertEqual(['a', 'c', 'b'], actual)

  def test_retry_inherited(self):
    # Exception class inheritance works.
    class CustomException(IOError):
      pass
    ran = []
    def throw(to_throw, x):
      ran.append(x)
      if to_throw:
        raise to_throw.pop(0)
      return x
    with threading_utils.AutoRetryThreadPool([IOError], 1, 1, 1, 0) as pool:
      pool.add_task(
          threading_utils.PRIORITY_MED, throw, [CustomException('a')], 'yay')
      actual = pool.join()
    self.assertEqual(['yay'], actual)
    self.assertEqual(['yay', 'yay'], ran)

  def test_retry_2_times(self):
    exceptions = [IOError, OSError]
    to_throw = [OSError('a'), IOError('b')]
    def throw(x):
      if to_throw:
        raise to_throw.pop(0)
      return x
    with threading_utils.AutoRetryThreadPool(exceptions, 2, 1, 1, 0) as pool:
      pool.add_task(threading_utils.PRIORITY_MED, throw, 'yay')
      actual = pool.join()
    self.assertEqual(['yay'], actual)

  def test_retry_too_many_times(self):
    exceptions = [IOError, OSError]
    to_throw = [OSError('a'), IOError('b')]
    def throw(x):
      if to_throw:
        raise to_throw.pop(0)
      return x
    with threading_utils.AutoRetryThreadPool(exceptions, 1, 1, 1, 0) as pool:
      pool.add_task(threading_utils.PRIORITY_MED, throw, 'yay')
      with self.assertRaises(IOError):
        pool.join()

  def test_retry_mutation_1(self):
    # This is to warn that mutable arguments WILL be mutated.
    def throw(to_throw, x):
      if to_throw:
        raise to_throw.pop(0)
      return x
    exceptions = [IOError, OSError]
    with threading_utils.AutoRetryThreadPool(exceptions, 1, 1, 1, 0) as pool:
      pool.add_task(
          threading_utils.PRIORITY_MED,
          throw,
          [OSError('a'), IOError('b')],
          'yay')
      with self.assertRaises(IOError):
        pool.join()

  def test_retry_mutation_2(self):
    # This is to warn that mutable arguments WILL be mutated.
    def throw(to_throw, x):
      if to_throw:
        raise to_throw.pop(0)
      return x
    exceptions = [IOError, OSError]
    with threading_utils.AutoRetryThreadPool(exceptions, 2, 1, 1, 0) as pool:
      pool.add_task(
          threading_utils.PRIORITY_MED,
          throw,
          [OSError('a'), IOError('b')],
          'yay')
      actual = pool.join()
    self.assertEqual(['yay'], actual)

  def test_retry_interleaved(self):
    # Verifies that retries are interleaved. This is important, we don't want a
    # retried task to take all the pool during retries.
    exceptions = [IOError, OSError]
    lock = threading.Lock()
    ran = []
    with threading_utils.AutoRetryThreadPool(exceptions, 2, 1, 1, 0) as pool:
      def lock_and_throw(to_throw, x):
        with lock:
          ran.append(x)
          if to_throw:
            raise to_throw.pop(0)
          return x
      with lock:
        pool.add_task(
            threading_utils.PRIORITY_MED,
            lock_and_throw,
            [OSError('a'), IOError('b')],
            'A')
        pool.add_task(
            threading_utils.PRIORITY_MED,
            lock_and_throw,
            [OSError('a'), IOError('b')],
            'B')

      actual = pool.join()
    self.assertEqual(['A', 'B'], actual)
    # Retries are properly interleaved:
    self.assertEqual(['A', 'B', 'A', 'B', 'A', 'B'], ran)

  def test_add_task_with_channel_success(self):
    with threading_utils.AutoRetryThreadPool([OSError], 2, 1, 1, 0) as pool:
      channel = threading_utils.TaskChannel()
      pool.add_task_with_channel(channel, 0, lambda: 0)
      self.assertEqual(0, channel.pull())

  def test_add_task_with_channel_fatal_error(self):
    with threading_utils.AutoRetryThreadPool([OSError], 2, 1, 1, 0) as pool:
      channel = threading_utils.TaskChannel()
      def throw(exc):
        raise exc
      pool.add_task_with_channel(channel, 0, throw, ValueError())
      with self.assertRaises(ValueError):
        channel.pull()

  def test_add_task_with_channel_retryable_error(self):
    with threading_utils.AutoRetryThreadPool([OSError], 2, 1, 1, 0) as pool:
      channel = threading_utils.TaskChannel()
      def throw(exc):
        raise exc
      pool.add_task_with_channel(channel, 0, throw, OSError())
      with self.assertRaises(OSError):
        channel.pull()

  def test_add_task_with_channel_captures_stack_trace(self):
    with threading_utils.AutoRetryThreadPool([OSError], 2, 1, 1, 0) as pool:
      channel = threading_utils.TaskChannel()
      def throw(exc):
        def function_with_some_unusual_name():
          raise exc
        function_with_some_unusual_name()
      pool.add_task_with_channel(channel, 0, throw, OSError())
      exc_traceback = ''
      try:
        channel.pull()
      except OSError:
        exc_traceback = traceback.format_exc()
      self.assertIn('function_with_some_unusual_name', exc_traceback)

  def test_max_value(self):
    self.assertEqual(16, threading_utils.IOAutoRetryThreadPool.MAX_WORKERS)


class FakeProgress(object):
  @staticmethod
  def print_update():
    pass


class WorkerPoolTest(unittest.TestCase):
  def test_normal(self):
    mapper = lambda value: -value
    progress = FakeProgress()
    with threading_utils.ThreadPoolWithProgress(progress, 8, 8, 0) as pool:
      for i in range(32):
        pool.add_task(0, mapper, i)
      results = pool.join()
    self.assertEqual(range(-31, 1), sorted(results))

  def test_exception(self):
    class FearsomeException(Exception):
      pass
    def mapper(value):
      raise FearsomeException(value)
    task_added = False
    try:
      progress = FakeProgress()
      with threading_utils.ThreadPoolWithProgress(progress, 8, 8, 0) as pool:
        pool.add_task(0, mapper, 0)
        task_added = True
        pool.join()
        self.fail()
    except FearsomeException:
      self.assertEqual(True, task_added)


class TaskChannelTest(unittest.TestCase):
  def test_passes_simple_value(self):
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      tp.add_task(0, lambda: channel.send_result(0))
      self.assertEqual(0, channel.pull())

  def test_passes_exception_value(self):
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      tp.add_task(0, lambda: channel.send_result(Exception()))
      self.assertTrue(isinstance(channel.pull(), Exception))

  def test_wrap_task_passes_simple_value(self):
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      tp.add_task(0, channel.wrap_task(lambda: 0))
      self.assertEqual(0, channel.pull())

  def test_wrap_task_passes_exception_value(self):
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      tp.add_task(0, channel.wrap_task(lambda: Exception()))
      self.assertTrue(isinstance(channel.pull(), Exception))

  def test_send_exception_raises_exception(self):
    class CustomError(Exception):
      pass
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      exc_info = (CustomError, CustomError(), None)
      tp.add_task(0, lambda: channel.send_exception(exc_info))
      with self.assertRaises(CustomError):
        channel.pull()

  def test_wrap_task_raises_exception(self):
    class CustomError(Exception):
      pass
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      def task_func():
        raise CustomError()
      tp.add_task(0, channel.wrap_task(task_func))
      with self.assertRaises(CustomError):
        channel.pull()

  def test_wrap_task_exception_captures_stack_trace(self):
    class CustomError(Exception):
      pass
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      def task_func():
        def function_with_some_unusual_name():
          raise CustomError()
        function_with_some_unusual_name()
      tp.add_task(0, channel.wrap_task(task_func))
      exc_traceback = ''
      try:
        channel.pull()
      except CustomError:
        exc_traceback = traceback.format_exc()
      self.assertIn('function_with_some_unusual_name', exc_traceback)

  def test_pull_timeout(self):
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      def task_func():
        # This test ultimately relies on the condition variable primitive
        # provided by pthreads. There's no easy way to mock time for it.
        # Increase this duration if the test is flaky.
        time.sleep(0.2)
        return 123
      tp.add_task(0, channel.wrap_task(task_func))
      with self.assertRaises(threading_utils.TaskChannel.Timeout):
        channel.pull(timeout=0.001)
      self.assertEqual(123, channel.pull())

  def test_timeout_exception_from_task(self):
    with threading_utils.ThreadPool(1, 1, 0) as tp:
      channel = threading_utils.TaskChannel()
      def task_func():
        raise threading_utils.TaskChannel.Timeout()
      tp.add_task(0, channel.wrap_task(task_func))
      # 'Timeout' raised by task gets transformed into 'RuntimeError'.
      with self.assertRaises(RuntimeError):
        channel.pull()


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
