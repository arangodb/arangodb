////////////////////////////////////////////////////////////////////////////////
/// @brief task for general communication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_GENERAL_SERVER_GENERAL_ASYNC_COMM_TASK_H
#define TRIAGENS_GENERAL_SERVER_GENERAL_ASYNC_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"

#include "Basics/Exceptions.h"

#include "Scheduler/AsyncTask.h"
#include "Rest/Handler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                        class GeneralAsyncCommTask
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief task for general communication
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename T>
    class GeneralAsyncCommTask : public T, public AsyncTask {
      private:
        GeneralAsyncCommTask (GeneralAsyncCommTask const&);
        GeneralAsyncCommTask& operator= (GeneralAsyncCommTask const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task with a given socket
////////////////////////////////////////////////////////////////////////////////

      public:

        GeneralAsyncCommTask (S* server, TRI_socket_t s, ConnectionInfo const& info, double keepAliveTimeout)
          : Task("GeneralAsyncCommTask"),
            T(server, s, info, keepAliveTimeout) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

      protected:

        ~GeneralAsyncCommTask () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      Task methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool setup (Scheduler* scheduler, EventLoop loop) {
          bool ok;
          ok = SocketTask::setup(scheduler, loop);
          if (!ok) {
            return false;
          }  
          ok = AsyncTask::setup(scheduler, loop);
          if (!ok) {
            return false;
          }
          return true;          
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void cleanup () {
          SocketTask::cleanup();
          AsyncTask::cleanup();
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleEvent (EventToken token, EventType events) {
          bool result = T::handleEvent(token, events);

          if (result) {
            result = AsyncTask::handleEvent(token, events);
          }

          return result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AsyncTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief handles the signal
////////////////////////////////////////////////////////////////////////////////

        bool handleAsync () {
          this->_server->handleAsync(this);
          return true;
        }
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
