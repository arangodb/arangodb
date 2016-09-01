////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "Scheduler/SchedulerLibev.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <ev.h>

#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Scheduler/SchedulerThread.h"
#include "Scheduler/Task.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

/* EV_TIMER is an alias for EV_TIMEOUT */
#ifndef EV_TIMER
#define EV_TIMER EV_TIMEOUT
#endif

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief waker callback
////////////////////////////////////////////////////////////////////////////////

void wakerCallback(struct ev_loop* loop, ev_async*, int) {
  ev_unloop(loop, EVUNLOOP_ALL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief socket event watcher
////////////////////////////////////////////////////////////////////////////////

struct SocketWatcher final : public ev_io, Watcher {
  struct ev_loop* loop;
  Task* task;

  SocketWatcher(struct ev_loop* loop, Task* task)
      : Watcher(EVENT_SOCKET_READ), loop(loop), task(task) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief socket event callback
////////////////////////////////////////////////////////////////////////////////

void socketCallback(struct ev_loop*, ev_io* w, int revents) {
  SocketWatcher* watcher = (SocketWatcher*)w;  // cast from C type to C++ class
  Task* task = watcher->task;

  if (task != nullptr) {
    if (revents & EV_READ) {
      if (revents & EV_WRITE) {
        // read and write
        task->handleEvent(watcher, EVENT_SOCKET_READ | EVENT_SOCKET_WRITE);
      } else {
        // read
        task->handleEvent(watcher, EVENT_SOCKET_READ);
      }
    } else if (revents & EV_WRITE) {
      // write
      task->handleEvent(watcher, EVENT_SOCKET_WRITE);
    }

    // note: task may have been destroyed by here, so it's not safe to access it
    // anymore
  } else {
    LOG(WARN) << "socketCallback called for unknown task";
    // TODO: given that the task is unknown, is it safe to stop to I/O here?
    // ev_io_stop(watcher->loop, w);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief periodic event watcher
////////////////////////////////////////////////////////////////////////////////

struct PeriodicWatcher final : public ev_periodic, Watcher {
  struct ev_loop* loop;
  Task* task;

  PeriodicWatcher(struct ev_loop* loop, Task* task)
      : Watcher(EVENT_PERIODIC), loop(loop), task(task) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief periodic event callback
////////////////////////////////////////////////////////////////////////////////

void periodicCallback(struct ev_loop*, ev_periodic* w, int revents) {
  PeriodicWatcher* watcher =
      (PeriodicWatcher*)w;  // cast from C type to C++ class
  Task* task = watcher->task;

  if (task != nullptr && (revents & EV_PERIODIC)) {
    task->handleEvent(watcher, EVENT_PERIODIC);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signal event watcher
////////////////////////////////////////////////////////////////////////////////

struct SignalWatcher final : public ev_signal, Watcher {
  struct ev_loop* loop;
  Task* task;

  SignalWatcher(struct ev_loop* loop, Task* task)
      : Watcher(EVENT_SIGNAL), loop(loop), task(task) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief signal event callback
////////////////////////////////////////////////////////////////////////////////

void signalCallback(struct ev_loop*, ev_signal* w, int revents) {
  SignalWatcher* watcher = (SignalWatcher*)w;  // cast from C type to C++ class
  Task* task = watcher->task;

  if (task != nullptr && (revents & EV_SIGNAL)) {
    task->handleEvent(watcher, EVENT_SIGNAL);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief timer event watcher
////////////////////////////////////////////////////////////////////////////////

struct TimerWatcher final : public ev_timer, Watcher {
  struct ev_loop* loop;
  Task* task;

  TimerWatcher(struct ev_loop* loop, Task* task)
      : Watcher(EVENT_TIMER), loop(loop), task(task) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief timer event callback
////////////////////////////////////////////////////////////////////////////////

void timerCallback(struct ev_loop*, ev_timer* w, int revents) {
  TimerWatcher* watcher = (TimerWatcher*)w;  // cast from C type to C++ class
  Task* task = watcher->task;

  if (task != nullptr && (revents & EV_TIMER)) {
    task->handleEvent(watcher, EVENT_TIMER);
  }
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the allocator was switched
////////////////////////////////////////////////////////////////////////////////

bool SchedulerLibev::SwitchedAllocator = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the available backends
////////////////////////////////////////////////////////////////////////////////

int SchedulerLibev::availableBackends() { return ev_supported_backends(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the libev allocator to our own allocator
///
/// this is done to avoid the numerous memory problems as reported by Valgrind
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::switchAllocator() {
  if (!SwitchedAllocator) {
    SwitchedAllocator = true;

    // set the libev allocator to our own allocator
    ev_set_allocator(
#ifdef EV_THROW
        reinterpret_cast<void* (*)(void* ptr, long size)EV_THROW>
#endif
        (&TRI_WrappedReallocate));
  }
}

static void LibEvErrorLogger(const char *msg) EV_THROW {
#if _WIN32
  TRI_ERRORBUF;
  TRI_SYSTEM_ERROR();
#endif
  LOG(WARN) << "LIBEV: " << msg << " - " << TRI_GET_ERRORBUF;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a scheduler
////////////////////////////////////////////////////////////////////////////////

SchedulerLibev::SchedulerLibev(size_t concurrency, int backend)
    : Scheduler(concurrency),
      _backend(backend),
      _loops(nullptr),
      _wakers(nullptr) {
  switchAllocator();

  //_backend = 1;

  // report status
  LOG(TRACE) << "supported backends: " << ev_supported_backends();
  LOG(TRACE) << "recommended backends: " << ev_recommended_backends();
  LOG(TRACE) << "embeddable backends: " << ev_embeddable_backends();
  LOG(TRACE) << "backend flags: " << backend;

  // construct the loops
  _loops = new struct ev_loop* [nrThreads];

  ev_set_syserr_cb(LibEvErrorLogger);

  ((struct ev_loop**)_loops)[0] = ev_default_loop(_backend);

  for (size_t i = 1; i < nrThreads; ++i) {
    ((struct ev_loop**)_loops)[i] = ev_loop_new(_backend);
  }

  // construct the scheduler threads
  threads = new SchedulerThread* [nrThreads];
  _wakers = new ev_async* [nrThreads];

  for (size_t i = 0; i < nrThreads; ++i) {
    threads[i] = new SchedulerThread(this, EventLoop(i), i == 0);

    ev_async* w = new ev_async;

    ev_async_init(w, wakerCallback);
    ev_async_start(((struct ev_loop**)_loops)[i], w);

    ((ev_async**)_wakers)[i] = w;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a scheduler
////////////////////////////////////////////////////////////////////////////////

SchedulerLibev::~SchedulerLibev() {
  // begin shutdown sequence within threads
  for (size_t i = 0; i < nrThreads; ++i) {
    threads[i]->beginShutdown();
  }
  
  for (size_t i = 0; i < nrThreads; ++i) {
    while (threads[i]->isRunning()) {
      usleep(1000);
    }
  }

  for (size_t i = 0; i < 100 && isRunning(); ++i) {
    usleep(1000);
  }

  // shutdown loops
  for (size_t i = 1; i < nrThreads; ++i) {
    ev_async_stop(((struct ev_loop**)_loops)[i], ((ev_async**)_wakers)[i]);
    ev_loop_destroy(((struct ev_loop**)_loops)[i]);
  }

  ev_async_stop(((struct ev_loop**)_loops)[0], ((ev_async**)_wakers)[0]);
  ev_default_destroy();

  // and delete threads
  for (size_t i = 0; i < nrThreads; ++i) {
    delete threads[i];
    delete ((ev_async**)_wakers)[i];
  }

  // delete loops buffer
  delete[]((struct ev_loop**)_loops);

  // delete threads buffer and wakers
  delete[] threads;
  delete[](ev_async**)_wakers;
}

void SchedulerLibev::eventLoop(EventLoop loop) {
  struct ev_loop* l = (struct ev_loop*)lookupLoop(loop);

  ev_loop(l, 0);
}

void SchedulerLibev::wakeupLoop(EventLoop loop) {
  if (size_t(loop) >= nrThreads) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unknown loop");
  }

  ev_async_send(((struct ev_loop**)_loops)[loop], ((ev_async**)_wakers)[loop]);
}

void SchedulerLibev::uninstallEvent(EventToken watcher) {
  if (watcher == nullptr) {
    return;
  }

  EventType type = watcher->type;

  switch (type) {
    case EVENT_PERIODIC: {
      PeriodicWatcher* w = (PeriodicWatcher*)watcher;
      ev_periodic_stop(w->loop, (ev_periodic*)w);
      delete w;

      break;
    }

    case EVENT_SIGNAL: {
      SignalWatcher* w = (SignalWatcher*)watcher;
      ev_signal_stop(w->loop, (ev_signal*)w);
      delete w;

      break;
    }

    case EVENT_SOCKET_READ: {
      SocketWatcher* w = (SocketWatcher*)watcher;
      ev_io_stop(w->loop, (ev_io*)w);
      delete w;

      break;
    }

    case EVENT_TIMER: {
      TimerWatcher* w = (TimerWatcher*)watcher;
      ev_timer_stop(w->loop, (ev_timer*)w);
      delete w;

      break;
    }
  }
}

EventToken SchedulerLibev::installPeriodicEvent(EventLoop loop, Task* task,
                                                double offset,
                                                double interval) {
  PeriodicWatcher* watcher =
      new PeriodicWatcher((struct ev_loop*)lookupLoop(loop), task);

  ev_periodic* w = (ev_periodic*)watcher;
  ev_periodic_init(w, periodicCallback, offset, interval, 0);
  ev_periodic_start(watcher->loop, w);

  return watcher;
}

void SchedulerLibev::rearmPeriodic(EventToken token, double offset,
                                   double interval) {
  PeriodicWatcher* watcher = (PeriodicWatcher*)token;

  if (watcher == nullptr) {
    return;
  }

  ev_periodic* w = (ev_periodic*)watcher;
  ev_periodic_set(w, offset, interval, 0);
  ev_periodic_again(watcher->loop, w);
}

EventToken SchedulerLibev::installSignalEvent(EventLoop loop, Task* task,
                                              int signal) {
  SignalWatcher* watcher =
      new SignalWatcher((struct ev_loop*)lookupLoop(loop), task);

  ev_signal* w = (ev_signal*)watcher;
  ev_signal_init(w, signalCallback, signal);
  ev_signal_start(watcher->loop, w);

  return watcher;
}

// ..........................................................................
// Windows likes to operate on SOCKET types (sort of handles) while libev
// likes to operate on file descriptors
// Our abstraction for sockets allows to use exactly the same code
// ..........................................................................

EventToken SchedulerLibev::installSocketEvent(EventLoop loop, EventType type,
                                              Task* task, TRI_socket_t socket) {
  SocketWatcher* watcher =
      new SocketWatcher((struct ev_loop*)lookupLoop(loop), task);

  int flags = 0;

  if (type & EVENT_SOCKET_READ) {
    flags |= EV_READ;
  }

  if (type & EVENT_SOCKET_WRITE) {
    flags |= EV_WRITE;
  }

  ev_io* w = (ev_io*)watcher;
  // Note that we do not use TRI_get_fd_or_handle_of_socket here because even
  // under Windows we want get the entry fileDescriptor here because
  // of the reason that is mentioned above!
  ev_io_init(w, socketCallback, socket.fileDescriptor, flags);
  ev_io_start(watcher->loop, w);

  return watcher;
}

void SchedulerLibev::startSocketEvents(EventToken token) {
  SocketWatcher* watcher = (SocketWatcher*)token;

  if (watcher == nullptr) {
    return;
  }

  ev_io* w = (ev_io*)watcher;

  // no need t ocheck if w is inactive, because ev_io_start()
  // will already do this
  ev_io_start(watcher->loop, w);
}

void SchedulerLibev::stopSocketEvents(EventToken token) {
  SocketWatcher* watcher = (SocketWatcher*)token;

  if (watcher == nullptr) {
    return;
  }

  ev_io* w = (ev_io*)watcher;

  // no need to check here if w is active, because ev_io_stop()
  // will already do this
  ev_io_stop(watcher->loop, w);
}

EventToken SchedulerLibev::installTimerEvent(EventLoop loop, Task* task,
                                             double timeout) {
  TimerWatcher* watcher =
      new TimerWatcher((struct ev_loop*)lookupLoop(loop), task);

  ev_timer* w = (ev_timer*)watcher;
  ev_timer_init(w, timerCallback, timeout, 0.0);
  ev_timer_start(watcher->loop, w);

  return watcher;
}

void SchedulerLibev::clearTimer(EventToken token) {
  TimerWatcher* watcher = (TimerWatcher*)token;

  if (watcher == nullptr) {
    return;
  }

  ev_timer* w = (ev_timer*)watcher;
  ev_timer_stop(watcher->loop, w);
}

void SchedulerLibev::rearmTimer(EventToken token, double timeout) {
  TimerWatcher* watcher = (TimerWatcher*)token;

  if (watcher == nullptr) {
    return;
  }

  ev_timer* w = (ev_timer*)watcher;
  ev_timer_set(w, 0.0, timeout);
  ev_timer_again(watcher->loop, w);
}

void SchedulerLibev::signalTask(std::unique_ptr<TaskData>& data) {
  size_t loop = size_t(data->_loop);

  if (loop >= nrThreads) {
    return;
  }

  threads[loop]->signalTask(data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an event lookup
////////////////////////////////////////////////////////////////////////////////

void* SchedulerLibev::lookupLoop(EventLoop loop) {
  if (size_t(loop) >= nrThreads) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unknown loop");
  }

  return ((struct ev_loop**)_loops)[loop];
}
