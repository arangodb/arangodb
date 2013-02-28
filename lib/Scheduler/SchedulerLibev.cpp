////////////////////////////////////////////////////////////////////////////////
/// @brief input-output scheduler using libev
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Scheduler/SchedulerLibev.h"

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#include <evwrap.h>
#else
#include <ev.h>
#endif

#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Scheduler/SchedulerThread.h"
#include "Scheduler/Task.h"

using namespace triagens::basics;
using namespace triagens::rest;

#ifdef TRI_USE_SPIN_LOCK_SCHEDULER_LIBEV
#define SCHEDULER_INIT TRI_InitSpin
#define SCHEDULER_DESTROY TRI_DestroySpin
#define SCHEDULER_LOCK TRI_LockSpin
#define SCHEDULER_UNLOCK TRI_UnlockSpin
#else
#define SCHEDULER_INIT TRI_InitMutex
#define SCHEDULER_DESTROY TRI_DestroyMutex
#define SCHEDULER_LOCK TRI_LockMutex
#define SCHEDULER_UNLOCK TRI_UnlockMutex
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                             libev
// -----------------------------------------------------------------------------

/* EV_TIMER is an alias for EV_TIMEOUT */
#ifndef EV_TIMER
#define EV_TIMER EV_TIMEOUT
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    private helper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief async event watcher
////////////////////////////////////////////////////////////////////////////////

  struct AsyncWatcher {
    ev_async async;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief async event callback
////////////////////////////////////////////////////////////////////////////////

  void asyncCallback (struct ev_loop*, ev_async* w, int revents) {
    AsyncWatcher* watcher = reinterpret_cast<AsyncWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_ASYNC) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_ASYNC);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief waker callback
////////////////////////////////////////////////////////////////////////////////

  void wakerCallback (struct ev_loop* loop, ev_async*, int) {
    ev_unloop(loop, EVUNLOOP_ALL);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief socket event watcher
////////////////////////////////////////////////////////////////////////////////

  struct SocketWatcher {
    ev_io io;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief socket event callback
////////////////////////////////////////////////////////////////////////////////

  void socketCallback (struct ev_loop*, ev_io* w, int revents) {
    SocketWatcher* watcher = reinterpret_cast<SocketWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && task->isActive()) {
      if (revents & EV_READ) {
        if (revents & EV_WRITE) {
          task->handleEvent(watcher->token, EVENT_SOCKET_READ | EVENT_SOCKET_WRITE);
        }
        else {
          task->handleEvent(watcher->token, EVENT_SOCKET_READ);
        }
      }
      else if (revents & EV_WRITE) {
        task->handleEvent(watcher->token, EVENT_SOCKET_WRITE);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief periodic event watcher
////////////////////////////////////////////////////////////////////////////////

  struct PeriodicWatcher {
    ev_periodic periodic;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief periodic event callback
////////////////////////////////////////////////////////////////////////////////

  void periodicCallback (struct ev_loop*, ev_periodic* w, int revents) {
    PeriodicWatcher* watcher = reinterpret_cast<PeriodicWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_PERIODIC) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_PERIODIC);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief signal event watcher
////////////////////////////////////////////////////////////////////////////////

  struct SignalWatcher {
    ev_signal signal;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief signal event callback
////////////////////////////////////////////////////////////////////////////////

  void signalCallback (struct ev_loop*, ev_signal* w, int revents) {
    SignalWatcher* watcher = reinterpret_cast<SignalWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_SIGNAL) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_SIGNAL);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief timer event watcher
////////////////////////////////////////////////////////////////////////////////

  struct TimerWatcher {
    ev_timer timer;
    struct ev_loop* loop;
    EventToken token;
    Task* task;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief timer event callback
////////////////////////////////////////////////////////////////////////////////

  void timerCallback (struct ev_loop*, ev_timer* w, int revents) {
    TimerWatcher* watcher = reinterpret_cast<TimerWatcher*>(w);
    Task* task = watcher->task;

    if (task != 0 && (revents & EV_TIMER) && task->isActive()) {
      task->handleEvent(watcher->token, EVENT_TIMER);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              class SchedulerLibev
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the available backends
////////////////////////////////////////////////////////////////////////////////

int SchedulerLibev::availableBackends () {
  return ev_supported_backends();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a scheduler
////////////////////////////////////////////////////////////////////////////////

SchedulerLibev::SchedulerLibev (size_t concurrency, int backend)
  : Scheduler(concurrency),
    _backend(backend) {

  //_backend = 1;
  // setup lock
  SCHEDULER_INIT(&_watcherLock);

  // report status
  LOGGER_TRACE << "supported backends: " << ev_supported_backends();
  LOGGER_TRACE << "recommended backends: " << ev_recommended_backends();
  LOGGER_TRACE << "embeddable backends: " << ev_embeddable_backends();
  LOGGER_TRACE << "backend flags: " << backend;
  
  // construct the loops
  _loops = new struct ev_loop*[nrThreads];

  ((struct ev_loop**) _loops)[0] = ev_default_loop(_backend);
  
  for (size_t i = 1;  i < nrThreads;  ++i) {
    ((struct ev_loop**) _loops)[i] = ev_loop_new(_backend);
  }
  
  // construct the scheduler threads
  threads = new SchedulerThread* [nrThreads];
  _wakers = new ev_async*[nrThreads];
  
  for (size_t i = 0;  i < nrThreads;  ++i) {
    threads[i] = new SchedulerThread(this, EventLoop(i), i == 0);
    
    ev_async* w = new ev_async;
    
    ev_async_init(w, wakerCallback);
    ev_async_start(((struct ev_loop**) _loops)[i], w);
    
    ((ev_async**) _wakers)[i] = w;
  }
  
  // watcher 0 is undefined
  _watchers.push_back(0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a scheduler
////////////////////////////////////////////////////////////////////////////////

SchedulerLibev::~SchedulerLibev () {

  // begin shutdown sequence within threads
  for (size_t i = 0;  i < nrThreads;  ++i) {
    threads[i]->beginShutdown();
  }
  
  // force threads to shutdown
  for (size_t i = 0;  i < nrThreads;  ++i) {
    threads[i]->stop();
  }
  
  for (size_t i = 0;  i < 100 && isRunning();  ++i) {
    usleep(100);
  }
  
  // shutdown loops
  for (size_t i = 1;  i < nrThreads;  ++i) {
    ev_async_stop(((struct ev_loop**) _loops)[i], ((ev_async**) _wakers)[i]);
    ev_loop_destroy(((struct ev_loop**) _loops)[i]);
  }
  
  ev_async_stop(((struct ev_loop**) _loops)[0], ((ev_async**) _wakers)[0]);
  ev_default_destroy();

  // and delete threads
  for (size_t i = 0;  i < nrThreads;  ++i) {
    delete threads[i];
    delete ((ev_async**) _wakers)[i];
  }
  
  // delete loops buffer
  delete[] ((struct ev_loop**) _loops);
  
  // delete threads buffer and wakers
  delete[] threads;
  delete[] (ev_async**)_wakers;

  // destroy lock
  SCHEDULER_DESTROY(&_watcherLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Scheduler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::eventLoop (EventLoop loop) {
  struct ev_loop* l = (struct ev_loop*) lookupLoop(loop);
  
  ev_loop(l, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::wakeupLoop (EventLoop loop) {
  if (size_t(loop) >= nrThreads) {
    THROW_INTERNAL_ERROR("unknown loop");
  }
  
  ev_async_send(((struct ev_loop**) _loops)[loop], ((ev_async**) _wakers)[loop]);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::uninstallEvent (EventToken token) {
  EventType type;
  void* watcher = lookupWatcher(token, type);
  
  if (watcher == 0) {
    return;
  }
  
  switch (type) {
    case EVENT_ASYNC: {
      AsyncWatcher* w = reinterpret_cast<AsyncWatcher*>(watcher);
      ev_async_stop(w->loop, (ev_async*) w);
      
      unregisterWatcher(token);
      delete w;
      
      break;
    }
      
    case EVENT_PERIODIC: {
      PeriodicWatcher* w = reinterpret_cast<PeriodicWatcher*>(watcher);
      ev_periodic_stop(w->loop, (ev_periodic*) w);
      
      unregisterWatcher(token);
      delete w;
      
      break;
    }
      
    case EVENT_SIGNAL: {
      SignalWatcher* w = reinterpret_cast<SignalWatcher*>(watcher);
      ev_signal_stop(w->loop, (ev_signal*) w);
      
      unregisterWatcher(token);
      delete w;
      
      break;
    }
      
    case EVENT_SOCKET_READ: {
      SocketWatcher* w = reinterpret_cast<SocketWatcher*>(watcher);
      ev_io_stop(w->loop, (ev_io*) w);
      
      unregisterWatcher(token);
      delete w;
      
      break;
    }
      
      
    case EVENT_TIMER: {
      TimerWatcher* w = reinterpret_cast<TimerWatcher*>(watcher);
      ev_timer_stop(w->loop, (ev_timer*) w);
      
      unregisterWatcher(token);
      delete w;
      
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

EventToken SchedulerLibev::installAsyncEvent (EventLoop loop, Task* task) {
  AsyncWatcher* watcher = new AsyncWatcher;
  watcher->loop = (struct ev_loop*) lookupLoop(loop);
  watcher->task = task;
  watcher->token = registerWatcher(watcher, EVENT_ASYNC);
  
  ev_async* w = (ev_async*) watcher;
  ev_async_init(w, asyncCallback);
  ev_async_start(watcher->loop, w);
  
  return watcher->token;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::sendAsync (EventToken token) {
  AsyncWatcher* watcher = reinterpret_cast<AsyncWatcher*>(lookupWatcher(token));
  
  if (watcher == 0) {
    return;
  }
  
  ev_async* w = (ev_async*) watcher;
  ev_async_send(watcher->loop, w);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

EventToken SchedulerLibev::installPeriodicEvent (EventLoop loop, Task* task, double offset, double interval) {
  PeriodicWatcher* watcher = new PeriodicWatcher;
  watcher->loop = (struct ev_loop*) lookupLoop(loop);
  watcher->task = task;
  watcher->token = registerWatcher(watcher, EVENT_PERIODIC);
  
  ev_periodic* w = (ev_periodic*) watcher;
  ev_periodic_init(w, periodicCallback, offset, interval, 0);
  ev_periodic_start(watcher->loop, w);
  
  return watcher->token;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::rearmPeriodic (EventToken token, double offset, double intervall) {
  PeriodicWatcher* watcher = reinterpret_cast<PeriodicWatcher*>(lookupWatcher(token));
  
  if (watcher == 0) {
    return;
  }
  
  ev_periodic* w = (ev_periodic*) watcher;
  ev_periodic_set(w, offset, intervall, 0);
  ev_periodic_again(watcher->loop, w);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

EventToken SchedulerLibev::installSignalEvent (EventLoop loop, Task* task, int signal) {
  SignalWatcher* watcher = new SignalWatcher;
  watcher->loop = (struct ev_loop*) lookupLoop(loop);
  watcher->task = task;
  watcher->token = registerWatcher(watcher, EVENT_SIGNAL);
  
  ev_signal* w = (ev_signal*) watcher;
  ev_signal_init(w, signalCallback, signal);
  ev_signal_start(watcher->loop, w);
  
  return watcher->token;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

  // ..........................................................................
  // Windows likes to operate on SOCKET types (sort of handles) while libev
  // likes to operate on file descriptors
  // ..........................................................................

  EventToken SchedulerLibev::installSocketEvent (EventLoop loop, EventType type, Task* task, TRI_socket_t socket) {
    SocketWatcher* watcher = new SocketWatcher;
    watcher->loop = (struct ev_loop*) lookupLoop(loop);
    watcher->task = task;

    int flags = 0;
  
    if (type & EVENT_SOCKET_READ) {
      flags |= EV_READ;
    }
  
    if (type & EVENT_SOCKET_WRITE) {
      flags |= EV_WRITE;
    }
   
    watcher->token = registerWatcher(watcher, EVENT_SOCKET_READ);
    ev_io* w = (ev_io*) watcher;
    ev_io_init(w, socketCallback, socket.fileDescriptor, flags);
    ev_io_start(watcher->loop, w);
    return watcher->token;
  }
#else
  EventToken SchedulerLibev::installSocketEvent (EventLoop loop, EventType type, Task* task, TRI_socket_t socket) {
    SocketWatcher* watcher = new SocketWatcher;
    watcher->loop = (struct ev_loop*) lookupLoop(loop);
    watcher->task = task;
    watcher->token = registerWatcher(watcher, EVENT_SOCKET_READ);

    int flags = 0;
  
    if (type & EVENT_SOCKET_READ) {
      flags |= EV_READ;
    }
  
    if (type & EVENT_SOCKET_WRITE) {
      flags |= EV_WRITE;
    }
  
    ev_io* w = (ev_io*) watcher;
    ev_io_init(w, socketCallback, socket.fileDescriptor, flags);
    ev_io_start(watcher->loop, w);

    return watcher->token;
  }
#endif

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::startSocketEvents (EventToken token) {
  SocketWatcher* watcher = reinterpret_cast<SocketWatcher*>(lookupWatcher(token));
  
  if (watcher == 0) {
    return;
  }
  
  ev_io* w = (ev_io*) watcher;
  
  if (! ev_is_active(w)) {
    ev_io_start(watcher->loop, w);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::stopSocketEvents (EventToken token) {
  SocketWatcher* watcher = reinterpret_cast<SocketWatcher*>(lookupWatcher(token));
  
  if (watcher == 0) {
    return;
  }
  
  ev_io* w = (ev_io*) watcher;
  
  if (ev_is_active(w)) {
    ev_io_stop(watcher->loop, w);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

EventToken SchedulerLibev::installTimerEvent (EventLoop loop, Task* task, double timeout) {
  TimerWatcher* watcher = new TimerWatcher;
  watcher->loop = (struct ev_loop*) lookupLoop(loop);
  watcher->task = task;
  watcher->token = registerWatcher(watcher, EVENT_TIMER);
  
  ev_timer* w = (ev_timer*) watcher;
  ev_timer_init(w, timerCallback, timeout, 0.0);
  ev_timer_start(watcher->loop, w);
  
  return watcher->token;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::clearTimer (EventToken token) {
  TimerWatcher* watcher = reinterpret_cast<TimerWatcher*>(lookupWatcher(token));
  
  if (watcher == 0) {
    return;
  }
  
  ev_timer* w = (ev_timer*) watcher;
  ev_timer_stop(watcher->loop, w);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::rearmTimer (EventToken token, double timeout) {
  TimerWatcher* watcher = reinterpret_cast<TimerWatcher*>(lookupWatcher(token));
  
  if (watcher == 0) {
    return;
  }
  
  ev_timer* w = (ev_timer*) watcher;
  ev_timer_set(w, 0.0, timeout);
  ev_timer_again(watcher->loop, w);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a watcher by event-token
////////////////////////////////////////////////////////////////////////////////

void* SchedulerLibev::lookupWatcher (EventToken token) {
  SCHEDULER_LOCK(&_watcherLock);

  if (token >= (EventToken) _watchers.size()) {
    SCHEDULER_UNLOCK(&_watcherLock);
    return 0;
  }
  
  void* watcher = _watchers[token];

  SCHEDULER_UNLOCK(&_watcherLock);
  return watcher;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a watcher by event-token and event-type
////////////////////////////////////////////////////////////////////////////////

void* SchedulerLibev::lookupWatcher (EventToken token, EventType& type) {
  SCHEDULER_LOCK(&_watcherLock);
  
  if (token >= (EventToken) _watchers.size()) {
    SCHEDULER_UNLOCK(&_watcherLock);
    return 0;
  }
  
  type = _types[token];
  void* watcher = _watchers[token];

  SCHEDULER_UNLOCK(&_watcherLock);
  return watcher;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an event lookup
////////////////////////////////////////////////////////////////////////////////

void* SchedulerLibev::lookupLoop (EventLoop loop) {
  if (size_t(loop) >= nrThreads) {
    THROW_INTERNAL_ERROR("unknown loop");
  }
  
  return ((struct ev_loop**) _loops)[loop];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a watcher
////////////////////////////////////////////////////////////////////////////////

EventToken SchedulerLibev::registerWatcher (void* watcher, EventType type) {
  SCHEDULER_LOCK(&_watcherLock);
  
  EventToken token;
  
  if (_frees.empty()) {
    token = _watchers.size();
    _watchers.push_back(watcher);
  }
  else {
    token = _frees.back();
    _frees.pop_back();
    _watchers[token] = watcher;
  }
  
  _types[token] = type;
  
  SCHEDULER_UNLOCK(&_watcherLock);
  return token;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a watcher
////////////////////////////////////////////////////////////////////////////////

void SchedulerLibev::unregisterWatcher (EventToken token) {
  SCHEDULER_LOCK(&_watcherLock);
  
  _frees.push_back(token);
  _watchers[token] = 0;

  SCHEDULER_UNLOCK(&_watcherLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
