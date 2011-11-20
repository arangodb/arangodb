////////////////////////////////////////////////////////////////////////////////
/// @brief task for specific communication
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

#ifndef FYN_GENERALSERVER_SPECIFIC_COMM_TASK_H
#define FYN_GENERALSERVER_SPECIFIC_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"

#include <Basics/Exceptions.h>

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief task for specific communication
    ////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename T>
    class SpecificCommTask : public T {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new task with a given socket
        ////////////////////////////////////////////////////////////////////////////////

        SpecificCommTask (S* server, socket_t fd, ConnectionInfo const& info)
          : Task("SpecificCommTask"),
            T(server, fd, info) {
        }
    };
  }
}

#endif
