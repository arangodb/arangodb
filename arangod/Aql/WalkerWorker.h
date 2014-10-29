////////////////////////////////////////////////////////////////////////////////
/// @brief recursive execution plan walker
///
/// @file 
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_WALKER_WORKER_H
#define ARANGODB_AQL_WALKER_WORKER_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to walk an execution plan recursively
////////////////////////////////////////////////////////////////////////////////

    template<class T> class WalkerWorker {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        WalkerWorker () {
        }

        virtual ~WalkerWorker () {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

        virtual bool before (T*) {
          return false; // true to abort the whole walking process
        }

        virtual void after (T*) {
        }

        virtual bool enterSubquery (T*, T*) { // super, sub
          return true;
        }

        virtual void leaveSubquery (T*, // super,
                                    T*  // sub
                                   ) {
        }

        bool done (T* en) {
          if (_done.find(en) == _done.end()) {
            _done.insert(en);
            return false;
          }

          return true;
        }

        void reset () {
          _done.clear();
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        std::unordered_set<T*> _done;
    };

  }
} 

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

