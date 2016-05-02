# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Classes and functions related to threading."""

import functools
import inspect
import logging
import os
import Queue
import sys
import threading
import time
import traceback


# Priorities for tasks in AutoRetryThreadPool, particular values are important.
PRIORITY_HIGH = 1 << 8
PRIORITY_MED = 2 << 8
PRIORITY_LOW = 3 << 8


class LockWithAssert(object):
  """Wrapper around (non recursive) Lock that tracks its owner."""

  def __init__(self):
    self._lock = threading.Lock()
    self._owner = None

  def __enter__(self):
    self._lock.acquire()
    assert self._owner is None
    self._owner = threading.current_thread()

  def __exit__(self, _exc_type, _exec_value, _traceback):
    self.assert_locked('Releasing unowned lock')
    self._owner = None
    self._lock.release()
    return False

  def assert_locked(self, msg=None):
    """Asserts the lock is owned by running thread."""
    assert self._owner == threading.current_thread(), msg


class ThreadPoolError(Exception):
  """Base class for exceptions raised by ThreadPool."""
  pass


class ThreadPoolEmpty(ThreadPoolError):
  """Trying to get task result from a thread pool with no pending tasks."""
  pass


class ThreadPoolClosed(ThreadPoolError):
  """Trying to do something with a closed thread pool."""
  pass


class ThreadPool(object):
  """Multithreaded worker pool with priority support.

  When the priority of tasks match, it works in strict FIFO mode.
  """
  QUEUE_CLASS = Queue.PriorityQueue

  def __init__(self, initial_threads, max_threads, queue_size, prefix=None):
    """Immediately starts |initial_threads| threads.

    Arguments:
      initial_threads: Number of threads to start immediately. Can be 0 if it is
                       uncertain that threads will be needed.
      max_threads: Maximum number of threads that will be started when all the
                   threads are busy working. Often the number of CPU cores.
      queue_size: Maximum number of tasks to buffer in the queue. 0 for
                  unlimited queue. A non-zero value may make add_task()
                  blocking.
      prefix: Prefix to use for thread names. Pool's threads will be
              named '<prefix>-<thread index>'.
    """
    prefix = prefix or 'tp-0x%0x' % id(self)
    logging.debug(
        'New ThreadPool(%d, %d, %d): %s', initial_threads, max_threads,
        queue_size, prefix)
    assert initial_threads <= max_threads
    assert max_threads <= 1024

    self.tasks = self.QUEUE_CLASS(queue_size)
    self._max_threads = max_threads
    self._prefix = prefix

    # Used to assign indexes to tasks.
    self._num_of_added_tasks_lock = threading.Lock()
    self._num_of_added_tasks = 0

    # Lock that protected everything below (including conditional variable).
    self._lock = threading.Lock()

    # Condition 'bool(_outputs) or bool(_exceptions) or _pending_count == 0'.
    self._outputs_exceptions_cond = threading.Condition(self._lock)
    self._outputs = []
    self._exceptions = []

    # Number of pending tasks (queued or being processed now).
    self._pending_count = 0

    # List of threads.
    self._workers = []
    # Number of threads that are waiting for new tasks.
    self._ready = 0
    # Number of threads already added to _workers, but not yet running the loop.
    self._starting = 0
    # True if close was called. Forbids adding new tasks.
    self._is_closed = False

    for _ in range(initial_threads):
      self._add_worker()

  def _add_worker(self):
    """Adds one worker thread if there isn't too many. Thread-safe."""
    with self._lock:
      if len(self._workers) >= self._max_threads or self._is_closed:
        return False
      worker = threading.Thread(
        name='%s-%d' % (self._prefix, len(self._workers)), target=self._run)
      self._workers.append(worker)
      self._starting += 1
    logging.debug('Starting worker thread %s', worker.name)
    worker.daemon = True
    worker.start()
    return True

  def add_task(self, priority, func, *args, **kwargs):
    """Adds a task, a function to be executed by a worker.

    Arguments:
    - priority: priority of the task versus others. Lower priority takes
                precedence.
    - func: function to run. Can either return a return value to be added to the
            output list or be a generator which can emit multiple values.
    - args and kwargs: arguments to |func|. Note that if func mutates |args| or
                       |kwargs| and that the task is retried, see
                       AutoRetryThreadPool, the retry will use the mutated
                       values.

    Returns:
      Index of the item added, e.g. the total number of enqueued items up to
      now.
    """
    assert isinstance(priority, int)
    assert callable(func)
    with self._lock:
      if self._is_closed:
        raise ThreadPoolClosed('Can not add a task to a closed ThreadPool')
      start_new_worker = (
        # Pending task count plus new task > number of available workers.
        self.tasks.qsize() + 1 > self._ready + self._starting and
        # Enough slots.
        len(self._workers) < self._max_threads
      )
      self._pending_count += 1
    with self._num_of_added_tasks_lock:
      self._num_of_added_tasks += 1
      index = self._num_of_added_tasks
    self.tasks.put((priority, index, func, args, kwargs))
    if start_new_worker:
      self._add_worker()
    return index

  def _run(self):
    """Worker thread loop. Runs until a None task is queued."""
    # Thread has started, adjust counters.
    with self._lock:
      self._starting -= 1
      self._ready += 1
    while True:
      try:
        task = self.tasks.get()
      finally:
        with self._lock:
          self._ready -= 1
      try:
        if task is None:
          # We're done.
          return
        _priority, _index, func, args, kwargs = task
        if inspect.isgeneratorfunction(func):
          for out in func(*args, **kwargs):
            self._output_append(out)
        else:
          out = func(*args, **kwargs)
          self._output_append(out)
      except Exception as e:
        logging.warning('Caught exception: %s', e)
        exc_info = sys.exc_info()
        logging.info(''.join(traceback.format_tb(exc_info[2])))
        with self._outputs_exceptions_cond:
          self._exceptions.append(exc_info)
          self._outputs_exceptions_cond.notifyAll()
      finally:
        try:
          # Mark thread as ready again, mark task as processed. Do it before
          # waking up threads waiting on self.tasks.join(). Otherwise they might
          # find ThreadPool still 'busy' and perform unnecessary wait on CV.
          with self._outputs_exceptions_cond:
            self._ready += 1
            self._pending_count -= 1
            if self._pending_count == 0:
              self._outputs_exceptions_cond.notifyAll()
          self.tasks.task_done()
        except Exception as e:
          # We need to catch and log this error here because this is the root
          # function for the thread, nothing higher will catch the error.
          logging.exception('Caught exception while marking task as done: %s',
                            e)

  def _output_append(self, out):
    if out is not None:
      with self._outputs_exceptions_cond:
        self._outputs.append(out)
        self._outputs_exceptions_cond.notifyAll()

  def join(self):
    """Extracts all the results from each threads unordered.

    Call repeatedly to extract all the exceptions if desired.

    Note: will wait for all work items to be done before returning an exception.
    To get an exception early, use get_one_result().
    """
    # TODO(maruel): Stop waiting as soon as an exception is caught.
    self.tasks.join()
    with self._outputs_exceptions_cond:
      if self._exceptions:
        e = self._exceptions.pop(0)
        raise e[0], e[1], e[2]
      out = self._outputs
      self._outputs = []
    return out

  def get_one_result(self):
    """Returns the next item that was generated or raises an exception if one
    occurred.

    Raises:
      ThreadPoolEmpty - no results available.
    """
    # Get first available result.
    for result in self.iter_results():
      return result
    # No results -> tasks queue is empty.
    raise ThreadPoolEmpty('Task queue is empty')

  def iter_results(self):
    """Yields results as they appear until all tasks are processed."""
    while True:
      # Check for pending results.
      result = None
      self._on_iter_results_step()
      with self._outputs_exceptions_cond:
        if self._exceptions:
          e = self._exceptions.pop(0)
          raise e[0], e[1], e[2]
        if self._outputs:
          # Remember the result to yield it outside of the lock.
          result = self._outputs.pop(0)
        else:
          # No pending tasks -> all tasks are done.
          if not self._pending_count:
            return
          # Some task is queued, wait for its result to appear.
          # Use non-None timeout so that process reacts to Ctrl+C and other
          # signals, see http://bugs.python.org/issue8844.
          self._outputs_exceptions_cond.wait(timeout=0.1)
          continue
      yield result

  def close(self):
    """Closes all the threads."""
    # Ensure no new threads can be started, self._workers is effectively
    # a constant after that and can be accessed outside the lock.
    with self._lock:
      if self._is_closed:
        raise ThreadPoolClosed('Can not close already closed ThreadPool')
      self._is_closed = True
    for _ in range(len(self._workers)):
      # Enqueueing None causes the worker to stop.
      self.tasks.put(None)
    for t in self._workers:
      # 'join' without timeout blocks signal handlers, spin with timeout.
      while t.is_alive():
        t.join(30)
    logging.debug(
      'Thread pool \'%s\' closed: spawned %d threads total',
      self._prefix, len(self._workers))

  def abort(self):
    """Empties the queue.

    To be used when the pool should stop early, like when Ctrl-C was detected.

    Returns:
      Number of tasks cancelled.
    """
    index = 0
    while True:
      try:
        self.tasks.get_nowait()
        self.tasks.task_done()
        index += 1
      except Queue.Empty:
        return index

  def _on_iter_results_step(self):
    pass

  def __enter__(self):
    """Enables 'with' statement."""
    return self

  def __exit__(self, _exc_type, _exc_value, _traceback):
    """Enables 'with' statement."""
    self.close()


class AutoRetryThreadPool(ThreadPool):
  """Automatically retries enqueued operations on exception."""
  # See also PRIORITY_* module-level constants.
  INTERNAL_PRIORITY_BITS = (1<<8) - 1

  def __init__(self, exceptions, retries, *args, **kwargs):
    """
    Arguments:
      exceptions: list of exception classes that can be retried on.
      retries: maximum number of retries to do.
    """
    assert exceptions and all(issubclass(e, Exception) for e in exceptions), (
        exceptions)
    assert 1 <= retries <= self.INTERNAL_PRIORITY_BITS
    super(AutoRetryThreadPool, self).__init__(*args, **kwargs)
    self._swallowed_exceptions = tuple(exceptions)
    self._retries = retries

  def add_task(self, priority, func, *args, **kwargs):
    """Tasks added must not use the lower priority bits since they are reserved
    for retries.
    """
    assert (priority & self.INTERNAL_PRIORITY_BITS) == 0
    return super(AutoRetryThreadPool, self).add_task(
        priority,
        self._task_executer,
        priority,
        None,
        func,
        *args,
        **kwargs)

  def add_task_with_channel(self, channel, priority, func, *args, **kwargs):
    """Tasks added must not use the lower priority bits since they are reserved
    for retries.
    """
    assert (priority & self.INTERNAL_PRIORITY_BITS) == 0
    return super(AutoRetryThreadPool, self).add_task(
        priority,
        self._task_executer,
        priority,
        channel,
        func,
        *args,
        **kwargs)

  def _task_executer(self, priority, channel, func, *args, **kwargs):
    """Wraps the function and automatically retry on exceptions."""
    try:
      result = func(*args, **kwargs)
      if channel is None:
        return result
      channel.send_result(result)
    # pylint: disable=catching-non-exception
    except self._swallowed_exceptions as e:
      # Retry a few times, lowering the priority.
      actual_retries = priority & self.INTERNAL_PRIORITY_BITS
      if actual_retries < self._retries:
        priority += 1
        logging.debug(
            'Swallowed exception \'%s\'. Retrying at lower priority %X',
            e, priority)
        super(AutoRetryThreadPool, self).add_task(
            priority,
            self._task_executer,
            priority,
            channel,
            func,
            *args,
            **kwargs)
        return
      if channel is None:
        raise
      channel.send_exception()
    except Exception:
      if channel is None:
        raise
      channel.send_exception()


class IOAutoRetryThreadPool(AutoRetryThreadPool):
  """Thread pool that automatically retries on IOError.

  Supposed to be used for IO bound tasks, and thus default maximum number of
  worker threads is independent of number of CPU cores.
  """
  # Initial and maximum number of worker threads.
  INITIAL_WORKERS = 2
  MAX_WORKERS = 16 if sys.maxsize > 2L**32 else 8
  RETRIES = 5

  def __init__(self):
    super(IOAutoRetryThreadPool, self).__init__(
        [IOError],
        self.RETRIES,
        self.INITIAL_WORKERS,
        self.MAX_WORKERS,
        0,
        'io')


class Progress(object):
  """Prints progress and accepts updates thread-safely."""
  def __init__(self, columns):
    """Creates a Progress bar that will updates asynchronously from the worker
    threads.

    Arguments:
      columns: list of tuple(name, initialvalue), defines both the number of
               columns and their initial values.
    """
    assert all(
        len(c) == 2 and isinstance(c[0], str) and isinstance(c[1], int)
        for c in columns), columns
    # Members to be used exclusively in the primary thread.
    self.use_cr_only = True
    self.unfinished_commands = set()
    self.start = time.time()
    self._last_printed_line = ''
    self._columns = [c[1] for c in columns]
    self._columns_lookup = dict((c[0], i) for i, c in enumerate(columns))
    # Setting it to True forces a print on the first print_update() call.
    self._value_changed = True

    # To be used in all threads.
    self._queued_updates = Queue.Queue()

  def update_item(self, name, raw=False, **kwargs):
    """Queue information to print out.

    Arguments:
      name: string to print out to describe something that was completed.
      raw: if True, prints the data without the header.
      raw: if True, prints the data without the header.
      <kwargs>: argument name is a name of a column. it's value is the increment
                to the column, value is usually 0 or 1.
    """
    assert isinstance(name, str)
    assert isinstance(raw, bool)
    assert all(isinstance(v, int) for v in kwargs.itervalues())
    args = [(self._columns_lookup[k], v) for k, v in kwargs.iteritems() if v]
    self._queued_updates.put((name, raw, args))

  def print_update(self):
    """Prints the current status."""
    # Flush all the logging output so it doesn't appear within this output.
    for handler in logging.root.handlers:
      handler.flush()

    got_one = False
    while True:
      try:
        name, raw, args = self._queued_updates.get_nowait()
      except Queue.Empty:
        break

      for k, v in args:
        self._columns[k] += v
      self._value_changed = bool(args)
      if not name:
        # Even if raw=True, there's nothing to print.
        continue

      got_one = True
      if raw:
        # Prints the data as-is.
        self._last_printed_line = ''
        sys.stdout.write('\n%s\n' % name.strip('\n'))
      else:
        line, self._last_printed_line = self._gen_line(name)
        sys.stdout.write(line)

    if not got_one and self._value_changed:
      # Make sure a line is printed in that case where statistics changes.
      line, self._last_printed_line = self._gen_line('')
      sys.stdout.write(line)
      got_one = True
    self._value_changed = False
    if got_one:
      # Ensure that all the output is flushed to prevent it from getting mixed
      # with other output streams (like the logging streams).
      sys.stdout.flush()

    if self.unfinished_commands:
      logging.debug('Waiting for the following commands to finish:\n%s',
                    '\n'.join(self.unfinished_commands))

  def _gen_line(self, name):
    """Generates the line to be printed."""
    next_line = ('[%s] %6.2fs %s') % (
        self._render_columns(), time.time() - self.start, name)
    # Fill it with whitespace only if self.use_cr_only is set.
    prefix = ''
    if self.use_cr_only and self._last_printed_line:
      prefix = '\r'
    if self.use_cr_only:
      suffix = ' ' * max(0, len(self._last_printed_line) - len(next_line))
    else:
      suffix = '\n'
    return '%s%s%s' % (prefix, next_line, suffix), next_line

  def _render_columns(self):
    """Renders the columns."""
    columns_as_str = map(str, self._columns)
    max_len = max(map(len, columns_as_str))
    return '/'.join(i.rjust(max_len) for i in columns_as_str)


class QueueWithProgress(Queue.PriorityQueue):
  """Implements progress support in join()."""
  def __init__(self, progress, *args, **kwargs):
    Queue.PriorityQueue.__init__(self, *args, **kwargs)
    self.progress = progress

  def task_done(self):
    """Contrary to Queue.task_done(), it wakes self.all_tasks_done at each task
    done.
    """
    with self.all_tasks_done:
      try:
        unfinished = self.unfinished_tasks - 1
        if unfinished < 0:
          raise ValueError('task_done() called too many times')
        self.unfinished_tasks = unfinished
        # This is less efficient, because we want the Progress to be updated.
        self.all_tasks_done.notify_all()
      except Exception as e:
        logging.exception('task_done threw an exception.\n%s', e)

  def wake_up(self):
    """Wakes up all_tasks_done.

    Unlike task_done(), do not substract one from self.unfinished_tasks.
    """
    # TODO(maruel): This is highly inefficient, since the listener is awaken
    # twice; once per output, once per task. There should be no relationship
    # between the number of output and the number of input task.
    with self.all_tasks_done:
      self.all_tasks_done.notify_all()

  def join(self):
    """Calls print_update() whenever possible."""
    self.progress.print_update()
    with self.all_tasks_done:
      while self.unfinished_tasks:
        self.progress.print_update()
        # Use a short wait timeout so updates are printed in a timely manner.
        # TODO(maruel): Find a way so Progress.queue and self.all_tasks_done
        # share the same underlying event so no polling is necessary.
        self.all_tasks_done.wait(0.1)
      self.progress.print_update()


class ThreadPoolWithProgress(ThreadPool):
  QUEUE_CLASS = QueueWithProgress

  def __init__(self, progress, *args, **kwargs):
    self.QUEUE_CLASS = functools.partial(self.QUEUE_CLASS, progress)
    super(ThreadPoolWithProgress, self).__init__(*args, **kwargs)

  def _output_append(self, out):
    """Also wakes up the listener on new completed test_case."""
    super(ThreadPoolWithProgress, self)._output_append(out)
    self.tasks.wake_up()

  def _on_iter_results_step(self):
    self.tasks.progress.print_update()


class DeadlockDetector(object):
  """Context manager that can detect deadlocks.

  It will dump stack frames of all running threads if its 'ping' method isn't
  called in time.

  Usage:
    with DeadlockDetector(timeout=60) as detector:
      for item in some_work():
        ...
        detector.ping()
        ...

  Arguments:
    timeout - maximum allowed time between calls to 'ping'.
  """

  def __init__(self, timeout):
    self.timeout = timeout
    self._thread = None
    # Thread stop condition. Also lock for shared variables below.
    self._stop_cv = threading.Condition()
    self._stop_flag = False
    # Time when 'ping' was called last time.
    self._last_ping = None
    # True if pings are coming on time.
    self._alive = True

  def __enter__(self):
    """Starts internal watcher thread."""
    assert self._thread is None
    self.ping()
    self._thread = threading.Thread(name='deadlock-detector', target=self._run)
    self._thread.daemon = True
    self._thread.start()
    return self

  def __exit__(self, *_args):
    """Stops internal watcher thread."""
    assert self._thread is not None
    with self._stop_cv:
      self._stop_flag = True
      self._stop_cv.notify()
    self._thread.join()
    self._thread = None
    self._stop_flag = False

  def ping(self):
    """Notify detector that main thread is still running.

    Should be called periodically to inform the detector that everything is
    running as it should.
    """
    with self._stop_cv:
      self._last_ping = time.time()
      self._alive = True

  def _run(self):
    """Loop that watches for pings and dumps threads state if ping is late."""
    with self._stop_cv:
      while not self._stop_flag:
        # Skipped deadline? Dump threads and switch to 'not alive' state.
        if self._alive and time.time() > self._last_ping + self.timeout:
          self.dump_threads(time.time() - self._last_ping, True)
          self._alive = False

        # Pings are on time?
        if self._alive:
          # Wait until the moment we need to dump stack traces.
          # Most probably some other thread will call 'ping' to move deadline
          # further in time. We don't bother to wake up after each 'ping',
          # only right before initial expected deadline.
          self._stop_cv.wait(self._last_ping + self.timeout - time.time())
        else:
          # Skipped some pings previously. Just periodically silently check
          # for new pings with some arbitrary frequency.
          self._stop_cv.wait(self.timeout * 0.1)

  @staticmethod
  def dump_threads(timeout=None, skip_current_thread=False):
    """Dumps stack frames of all running threads."""
    all_threads = threading.enumerate()
    current_thread_id = threading.current_thread().ident

    # Collect tracebacks: thread name -> traceback string.
    tracebacks = {}

    # pylint: disable=W0212
    for thread_id, frame in sys._current_frames().iteritems():
      # Don't dump deadlock detector's own thread, it's boring.
      if thread_id == current_thread_id and skip_current_thread:
        continue

      # Try to get more informative symbolic thread name.
      name = 'untitled'
      for thread in all_threads:
        if thread.ident == thread_id:
          name = thread.name
          break
      name += ' #%d' % (thread_id,)
      tracebacks[name] = ''.join(traceback.format_stack(frame))

    # Function to print a message. Makes it easier to change output destination.
    def output(msg):
      logging.warning(msg.rstrip())

    # Print tracebacks, sorting them by thread name. That way a thread pool's
    # threads will be printed as one group.
    output('=============== Potential deadlock detected ===============')
    if timeout is not None:
      output('No pings in last %d sec.' % (timeout,))
    output('Dumping stack frames for all threads:')
    for name in sorted(tracebacks):
      output('Traceback for \'%s\':\n%s' % (name, tracebacks[name]))
    output('===========================================================')


class TaskChannel(object):
  """Queue of results of async task execution."""

  class Timeout(Exception):
    """Raised by 'pull' in case of timeout."""

  _ITEM_RESULT = 0
  _ITEM_EXCEPTION = 1

  def __init__(self):
    self._queue = Queue.Queue()

  def send_result(self, result):
    """Enqueues a result of task execution."""
    self._queue.put((self._ITEM_RESULT, result))

  def send_exception(self, exc_info=None):
    """Enqueue an exception raised by a task.

    Arguments:
      exc_info: If given, should be 3-tuple returned by sys.exc_info(),
                default is current value of sys.exc_info(). Use default in
                'except' blocks to capture currently processed exception.
    """
    exc_info = exc_info or sys.exc_info()
    assert isinstance(exc_info, tuple) and len(exc_info) == 3
    # Transparently passing Timeout will break 'pull' contract, since a caller
    # has no way to figure out that's an exception from the task and not from
    # 'pull' itself. Transform Timeout into generic RuntimeError with
    # explanation.
    if isinstance(exc_info[1], TaskChannel.Timeout):
      exc_info = (
          RuntimeError,
          RuntimeError('Task raised Timeout exception'),
          exc_info[2])
    self._queue.put((self._ITEM_EXCEPTION, exc_info))

  def pull(self, timeout=None):
    """Dequeues available result or exception.

    Args:
      timeout: if not None will block no longer than |timeout| seconds and will
          raise TaskChannel.Timeout exception if no results are available.

    Returns:
      Whatever task pushes to the queue by calling 'send_result'.

    Raises:
      TaskChannel.Timeout: waiting longer than |timeout|.
      Whatever exception task raises.
    """
    # Do not ever use timeout == None, in that case signal handlers are not
    # being called (at least on Python 2.7, http://bugs.python.org/issue8844).
    while True:
      try:
        item_type, value = self._queue.get(
            timeout=timeout if timeout is not None else 30.0)
        break
      except Queue.Empty:
        if timeout is None:
          continue
        raise TaskChannel.Timeout()
    if item_type == self._ITEM_RESULT:
      return value
    if item_type == self._ITEM_EXCEPTION:
      # 'value' is captured sys.exc_info() 3-tuple. Use extended raise syntax
      # to preserve stack frame of original exception (that was raised in
      # another thread).
      assert isinstance(value, tuple) and len(value) == 3
      raise value[0], value[1], value[2]
    assert False, 'Impossible queue item type: %r' % item_type

  def wrap_task(self, task):
    """Decorator that makes a function push results into this channel."""
    @functools.wraps(task)
    def wrapped(*args, **kwargs):
      try:
        self.send_result(task(*args, **kwargs))
      except Exception:
        self.send_exception()
    return wrapped


def num_processors():
  """Returns the number of processors.

  Python on OSX 10.6 raises a NotImplementedError exception.
  """
  try:
    # Multiprocessing
    import multiprocessing
    return multiprocessing.cpu_count()
  except:  # pylint: disable=W0702
    try:
      # Mac OS 10.6
      return int(os.sysconf('SC_NPROCESSORS_ONLN'))  # pylint: disable=E1101
    except:
      # Some of the windows builders seem to get here.
      return 4
