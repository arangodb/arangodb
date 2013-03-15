////////////////////////////////////////////////////////////////////////////////
/// @brief Condition Variable
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_CONDITION_VARIABLE_H
#define TRIAGENS_BASICS_CONDITION_VARIABLE_H 1

#include "Basics/Common.h"

#include "BasicsC/locks.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                           class ConditionVariable
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable
///
/// A condition variable consists of a condition and a monitor.
///
/// There are only two operations that can be applied to a condition variable:
/// wait and signal. When a thread executes a wait call in the monitor on a
/// condition variable, it is immediately suspended and put into the waiting
/// queue of that condition variable. Thus, this thread is suspended and is
/// waiting for the event that is represented by the condition variable to
/// occur. As the calling thread is the only thread that is running in the
/// monitor, it "owns" the monitor lock. When it is put into the waiting queue
/// of a condition variable, the system will automatically take the monitor lock
/// back. As a result, the monitor becomes empty and another thread can enter.
///
/// Eventually, a thread will cause the event to occur. To indicate a particular
/// event occurs, a thread calls the signal method on the corresponding
/// condition variable. At this point, we have two cases to consider. First, if
/// there are threads waiting on the signaled condition variable, the monitor
/// will allow one of the waiting threads to resume its execution and give this
/// thread the monitor lock back. Second, if there is no waiting thread on the
/// signaled condition variable, this signal is lost as if it never occurs.
///
/// Therefore, wait and signal for a condition variable is very similar to the
/// notification technique of a semaphore: One thread waits on an event and
/// resumes its execution when another thread causes the event to
/// occur. However, there are major differences as will be discussed on a later
/// page.
////////////////////////////////////////////////////////////////////////////////

    class  ConditionVariable {
        ConditionVariable (ConditionVariable const&);
        ConditionVariable& operator= (ConditionVariable const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a condition variable
////////////////////////////////////////////////////////////////////////////////

        ConditionVariable ();

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes the condition variable
////////////////////////////////////////////////////////////////////////////////

        ~ConditionVariable ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the condition variable
////////////////////////////////////////////////////////////////////////////////

        void lock ();

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the lock on the condition variable
////////////////////////////////////////////////////////////////////////////////

        void unlock ();

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for an event
////////////////////////////////////////////////////////////////////////////////

        void wait ();

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for an event with timeout in milli seconds
////////////////////////////////////////////////////////////////////////////////

        bool wait (uint64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief signals all waiting threads
////////////////////////////////////////////////////////////////////////////////

        void broadcast ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable
////////////////////////////////////////////////////////////////////////////////

        TRI_condition_t _condition;
    };
  }
}

#endif
