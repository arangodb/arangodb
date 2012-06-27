////////////////////////////////////////////////////////////////////////////////
/// @brief general server batch job
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
/// @author Jan Steemann
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_FYN_GENERAL_SERVER_GENERAL_SERVER_BATCH_JOB_H
#define TRIAGENS_FYN_GENERAL_SERVER_GENERAL_SERVER_BATCH_JOB_H 1

#include <Basics/Common.h>

#include <Logger/Logger.h>
#include <Rest/Handler.h>

#include "Dispatcher/Job.h"
#include "GeneralServer/GeneralServerJob.h"
#include "Scheduler/AsyncTask.h"

namespace triagens {
  namespace rest {
    class Dispatcher;
    class Scheduler;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief general server batch job
    ////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename H>
    class GeneralServerBatchJob : public GeneralServerJob<S, H> {
      GeneralServerBatchJob (GeneralServerBatchJob const&);
      GeneralServerBatchJob& operator= (GeneralServerBatchJob const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new server batch job
        ////////////////////////////////////////////////////////////////////////////////

        GeneralServerBatchJob (S* server, Scheduler* scheduler, Dispatcher* dispatcher, AsyncTask* task, H* handler)
          : GeneralServerJob<S, H>(server, scheduler, dispatcher, task, handler) {
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a server job
        ////////////////////////////////////////////////////////////////////////////////

        ~GeneralServerBatchJob () {
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void cleanup () {
          // do nothing in cleanup
        }
        
    };
  }
}

#endif
