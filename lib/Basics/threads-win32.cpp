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
////////////////////////////////////////////////////////////////////////////////

#include "threads.h"

#include "Logger/Logger.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief data block for thread starter
////////////////////////////////////////////////////////////////////////////////

struct thread_data_t {
  void (*_starter)(void*);
  void* _data;
  std::string _name;
  
  thread_data_t(void (*starter)(void*), void* data, char const* name) 
      : _starter(starter),
        _data(data),
        _name(name) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starter function for thread
////////////////////////////////////////////////////////////////////////////////

static DWORD __stdcall ThreadStarter(void* data) {
  TRI_ASSERT(data != nullptr);

  // this will automatically free the thread struct when leaving this function
  std::unique_ptr<thread_data_t> d(static_cast<thread_data_t*>(data));

  try {
    d->_starter(d->_data);
  } catch (...) {
    // we must not throw from here
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a thread
////////////////////////////////////////////////////////////////////////////////

void TRI_InitThread(TRI_thread_t* thread) { *thread = 0; }

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a thread
////////////////////////////////////////////////////////////////////////////////

bool TRI_StartThread(TRI_thread_t* thread, TRI_tid_t* threadId,
                     char const* name, void (*starter)(void*), void* data) {
  std::unique_ptr<thread_data_t> d;

  try {
    d.reset(new thread_data_t(starter, data, name));
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::THREADS) << "could not start thread: out of memory";
    return false;
  }

  TRI_ASSERT(d != nullptr);
  *thread = CreateThread(0,              // default security attributes
                         0,              // use default stack size
                         ThreadStarter,  // thread function name
                         d.release(),    // argument to thread function
                         0,              // use default creation flags
                         threadId);      // returns the thread identifier

  if (*thread == 0) {
    LOG_TOPIC(ERR, arangodb::Logger::THREADS) << "could not start thread: " << strerror(errno) << " ";
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a thread to finish
////////////////////////////////////////////////////////////////////////////////

int TRI_JoinThread(TRI_thread_t* thread) {
  DWORD result = WaitForSingleObject(*thread, INFINITE);

  switch (result) {
    case WAIT_ABANDONED: {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not join thread --> WAIT_ABANDONED"; 
      break;
    }

    case WAIT_OBJECT_0: {
      // everything ok
      break;
    }

    case WAIT_TIMEOUT: {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not join thread --> WAIT_TIMEOUT"; 
      break;
    }

    case WAIT_FAILED: {
      result = GetLastError();
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not join thread --> WAIT_FAILED - reason -->" << result; 
      break;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief detaches a thread 
////////////////////////////////////////////////////////////////////////////////

int TRI_DetachThread(TRI_thread_t* thread) {
  // If the function succeeds, the return value is nonzero.
  // If the function fails, the return value is zero. To get extended error information, call GetLastError. 
  BOOL res = CloseHandle(thread);

  if (res == 0) { 
    DWORD result = GetLastError();
    LOG_TOPIC(ERR, arangodb::Logger::THREADS) << "cannot detach thread: " << result;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if this thread is the thread passed as a parameter
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSelfThread(TRI_thread_t* thread) {
  // ...........................................................................
  // The GetThreadID(...) function is only available in Windows Vista or Higher
  // TODO: Change the TRI_thread_t into a structure which stores the thread id
  // as well as the thread handle. This can then be passed around
  // ...........................................................................
  return (GetCurrentThreadId() == GetThreadId(thread));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allow cancelation
////////////////////////////////////////////////////////////////////////////////

void TRI_AllowCancelation(void) {
  // TODO: No native implementation of this
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void TRI_SetProcessorAffinity(TRI_thread_t* thread, size_t core) {}
