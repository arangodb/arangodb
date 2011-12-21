////////////////////////////////////////////////////////////////////////////////
/// @brief Thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Thread.h"

#include <errno.h>
#include <signal.h>

#include <Basics/ConditionLocker.h>
#include <Basics/Logger.h>

using namespace triagens::basics;

void* Thread::startThread (void* arg) {
  sigset_t all;
  sigfillset(&all);
  
  pthread_sigmask(SIG_SETMASK, &all, 0);
  
  Thread * ptr = (Thread *) arg;
  
  ptr->runMe();
  ptr->cleanup();
  
  return 0;
}



namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // static public methods
    // -----------------------------------------------------------------------------

    TRI_pid_t Thread::currentProcessId () {
      return TRI_CurrentProcessId();
    }



    TRI_pid_t Thread::currentThreadProcessId () {
      return TRI_CurrentThreadProcessId();
    }



    TRI_tid_t Thread::currentThreadId () {
      return TRI_CurrentThreadId();
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    Thread::Thread (const string& name)
      : _name(name),
        _asynchronousCancelation(false),
        _thread(0),
        _finishedCondition(0),
        _started(0),
        _running(0) {
      memset(&_thread, 0, sizeof(_thread));
    }



    Thread::~Thread () {
      if (_running != 0) {
        LOGGER_WARNING << "forcefully shuting down thread '" << _name << "'";
        pthread_cancel(_thread);
      }

      pthread_detach(_thread);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool Thread::isRunning () {
      return _running != 0;
    }



    intptr_t Thread::threadId () {
      return (intptr_t) _thread;
    }



    bool Thread::start (ConditionVariable * finishedCondition) {
      _finishedCondition = finishedCondition;

      if (_started != 0) {
        LOGGER_FATAL << "called started on an already started thread";
        return false;
      }

      _started = 1;

      int rc = pthread_create(&_thread, 0, &startThread, this);

      if (rc != 0) {
        LOGGER_ERROR << "could not start thread '" << _name << "': " << strerror(errno);
        return false;
      }

      return true;
    }



    void Thread::stop () {
      if (_running != 0) {
        LOGGER_TRACE << "trying to cancel (aka stop) the thread " << _name;
        pthread_cancel(_thread);
      }
      else {
        LOGGER_DEBUG << "trying to cancel (aka stop) stopped thread " << _name;
      }
    }



    void Thread::join () {
      TRI_JoinThread(&_thread);
    }



    void Thread::sendSignal (int signal) {
      if (_running != 0) {
        int rc = pthread_kill(_thread, signal);

        if (rc != 0) {
          LOGGER_ERROR << "could not send signal to thread '" << _name << "': " << strerror(errno);
        }
      }
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    void Thread::allowAsynchronousCancelation () {
      if (_started) {
        if (_running) {
          if (_thread == pthread_self()) {
            LOGGER_DEBUG << "set asynchronous cancelation for " << _name;
            pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
          }
          else {
            LOGGER_ERROR << "cannot change cancelation type of an already running thread from the outside";
          }
        }
        else {
          LOGGER_WARNING << "thread has already stop, it is useless to change the cancelation type";
        }
      }
      else {
        _asynchronousCancelation = true;
      }
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    void Thread::runMe () {
      if (_asynchronousCancelation) {
        LOGGER_DEBUG << "set asynchronous cancelation for " << _name;
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
      }

      _running = 1;

      try {
        run();
      }
      catch (...) {
        LOGGER_DEBUG << "caught exception on " << _name;
        _running = 0;

        if (_finishedCondition != 0) {
          CONDITION_LOCKER(locker, *_finishedCondition);
          locker.broadcast();
        }

        throw;
      }

      _running = 0;

      if (_finishedCondition != 0) {
        CONDITION_LOCKER(locker, *_finishedCondition);
        locker.broadcast();
      }
    }
  }
}
