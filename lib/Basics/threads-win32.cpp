////////////////////////////////////////////////////////////////////////////////
/// @brief threads in win32 & win64
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "threads.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                            THREAD
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief data block for thread starter
////////////////////////////////////////////////////////////////////////////////

typedef struct thread_data_s {
  void (*starter) (void*);
  void* _data;
  char* _name;
}
thread_data_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief starter function for thread
////////////////////////////////////////////////////////////////////////////////

static DWORD __stdcall ThreadStarter (void* data) {
  thread_data_t* d;

  d = (thread_data_t*) data;
  d->starter(d->_data);

  TRI_Free(TRI_CORE_MEM_ZONE, d);

  return 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a thread
////////////////////////////////////////////////////////////////////////////////

void TRI_InitThread (TRI_thread_t* thread) {
  *thread = 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current process identifier
////////////////////////////////////////////////////////////////////////////////

TRI_pid_t TRI_CurrentProcessId () {
  return _getpid();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current thread process identifier
////////////////////////////////////////////////////////////////////////////////

TRI_tpid_t TRI_CurrentThreadProcessId () {
  return _getpid();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current thread identifier
////////////////////////////////////////////////////////////////////////////////

TRI_tid_t TRI_CurrentThreadId () {
  return GetCurrentThreadId();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a thread
////////////////////////////////////////////////////////////////////////////////

bool TRI_StartThread (TRI_thread_t* thread, TRI_tid_t* threadId, const char* name, void (*starter)(void*), void* data) {
  thread_data_t* d = static_cast<thread_data_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(thread_data_t), false));

  if (d == nullptr) {
    return false;
  }

  d->starter = starter;
  d->_data = data;
  d->_name = TRI_DuplicateString(name);

  *thread = CreateThread(0, // default security attributes
                         0, // use default stack size
                         ThreadStarter, // thread function name
                         d, // argument to thread function
                         0, // use default creation flags
                         threadId); // returns the thread identifier

  if (*thread == 0) {
    TRI_Free(TRI_CORE_MEM_ZONE, d);
    LOG_ERROR("could not start thread: %s ", strerror(errno));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to stop/terminate a thread
////////////////////////////////////////////////////////////////////////////////

int TRI_StopThread (TRI_thread_t* thread) {
  if (TerminateThread(*thread, 0) == 0) {
    DWORD result = GetLastError();

    LOG_ERROR("threads-win32.c:TRI_StopThread:could not stop thread -->%d", result);

    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a thread to finish
////////////////////////////////////////////////////////////////////////////////

int TRI_DetachThread (TRI_thread_t* thread) {
  // TODO: no native implementation
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a thread to finish
////////////////////////////////////////////////////////////////////////////////

int TRI_JoinThread (TRI_thread_t* thread) {
  DWORD result = WaitForSingleObject(*thread, INFINITE);

  switch (result) {
    case WAIT_ABANDONED: {
      LOG_FATAL_AND_EXIT("threads-win32.c:TRI_JoinThread:could not join thread --> WAIT_ABANDONED");
    }

    case WAIT_OBJECT_0: {
      // everything ok
      break;
    }

    case WAIT_TIMEOUT: {
      LOG_FATAL_AND_EXIT("threads-win32.c:TRI_JoinThread:could not joint thread --> WAIT_TIMEOUT");
    }

    case WAIT_FAILED: {
      result = GetLastError();
      LOG_FATAL_AND_EXIT("threads-win32.c:TRI_JoinThread:could not join thread --> WAIT_FAILED - reason -->%d",result);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends a signal to a thread
////////////////////////////////////////////////////////////////////////////////

bool TRI_SignalThread (TRI_thread_t* thread, int signum) {
  // TODO:  NO NATIVE implementation of signals
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if this thread is the thread passed as a parameter
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSelfThread (TRI_thread_t* thread) {
  return false;
  // ...........................................................................
  // The GetThreadID(...) function is only available in Windows Vista or Higher
  // TODO: Change the TRI_thread_t into a structure which stores the thread id
  // as well as the thread handle. This can then be passed around
  // ...........................................................................
  //return ( GetCurrentThreadId() == GetThreadId(thread) );
}


////////////////////////////////////////////////////////////////////////////////
/// @brief allow cancellation
////////////////////////////////////////////////////////////////////////////////

void TRI_AllowCancelation(void) {
  // TODO: No native implementation of this
}




// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
