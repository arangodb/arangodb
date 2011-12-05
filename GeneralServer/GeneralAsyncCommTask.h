////////////////////////////////////////////////////////////////////////////////
/// @brief task for general communication
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FYN_GENERAL_SERVER_GENERAL_ASYNC_COMM_TASK_H
#define TRIAGENS_FYN_GENERAL_SERVER_GENERAL_ASYNC_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"

#include <Basics/Exceptions.h>
#include <Rest/AsyncTask.h>

#include "GeneralServer/GeneralServerJob.h"

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief task for general communication
    ////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename T>
    class GeneralAsyncCommTask : public T, public AsyncTask {
      GeneralAsyncCommTask (GeneralAsyncCommTask const&);
      GeneralAsyncCommTask& operator= (GeneralAsyncCommTask const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new task with a given socket
        ////////////////////////////////////////////////////////////////////////////////

        GeneralAsyncCommTask (S* server, socket_t fd, ConnectionInfo const& info)
          : Task("GeneralAsyncCommTask"),
            T(server, fd, info),
            job(0) {
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets a job
        ////////////////////////////////////////////////////////////////////////////////

        void setJob (GeneralServerJob<S, typename HF::GeneralHandler>* job) {
          this->job = job;
        }

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a task
        ////////////////////////////////////////////////////////////////////////////////

        ~GeneralAsyncCommTask () {
          if (job != 0) {
            LOGGER_DEBUG << "job is still active, trying to shutdown";
            job->beginShutdown();
          }
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void setup (Scheduler* scheduler, EventLoop loop) {
          SocketTask::setup(scheduler, loop);
          AsyncTask::setup(scheduler, loop);
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

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief handles the signal
        ////////////////////////////////////////////////////////////////////////////////

        bool handleAsync () {
          if (job == 0) {
            LOGGER_WARNING << "no job is known";
          }
          else {
            typename HF::GeneralHandler * handler = job->getHandler();
            typename HF::GeneralResponse * response = handler->getResponse();

            try {
              if (response == 0) {
                basics::InternalError err("no response received from handler");

                handler->handleError(err);
                response = handler->getResponse();
              }

              if (response != 0) {
                this->handleResponse(response);
              }
            }
            catch (...) {
              LOGGER_ERROR << "caught exception in " << __FILE__ << "@" << __LINE__;
            }

            delete job;
            job = 0;
          }

          return true;
        }

      private:
        GeneralServerJob<S, typename HF::GeneralHandler>* job;
    };
  }
}

#endif
