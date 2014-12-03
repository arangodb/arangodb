////////////////////////////////////////////////////////////////////////////////
/// @brief AQL, data-modification query options
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

#ifndef ARANGODB_AQL_MODIFICATION_OPTIONS_H
#define ARANGODB_AQL_MODIFICATION_OPTIONS_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief ModificationOptions
////////////////////////////////////////////////////////////////////////////////

    struct ModificationOptions {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, using default values
////////////////////////////////////////////////////////////////////////////////

      ModificationOptions (triagens::basics::Json const& json);

      ModificationOptions ()
        : ignoreErrors(false),
          waitForSync(false),
          nullMeansRemove(false),
          mergeObjects(false) {
      }

      void toJson (triagens::basics::Json& json, TRI_memory_zone_t* zone) const;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      bool ignoreErrors;
      bool waitForSync;
      bool nullMeansRemove;
      bool mergeObjects;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

