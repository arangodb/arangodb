////////////////////////////////////////////////////////////////////////////////
/// @brief AQL, options for COLLECT
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

#ifndef ARANGODB_AQL_AGGREGATION_OPTIONS_H
#define ARANGODB_AQL_AGGREGATION_OPTIONS_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief CollectOptions
////////////////////////////////////////////////////////////////////////////////

    struct CollectOptions {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief selected aggregation method
////////////////////////////////////////////////////////////////////////////////
        
      enum CollectMethod {
        COLLECT_METHOD_UNDEFINED,
        COLLECT_METHOD_HASH,
        COLLECT_METHOD_SORTED
      };

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------
        
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, using default values
////////////////////////////////////////////////////////////////////////////////

      CollectOptions ()
        : method(COLLECT_METHOD_UNDEFINED) {
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor, using JSON
////////////////////////////////////////////////////////////////////////////////
      
      CollectOptions (triagens::basics::Json const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the hash method can be used
////////////////////////////////////////////////////////////////////////////////

      bool canUseHashMethod () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the options to JSON
////////////////////////////////////////////////////////////////////////////////

      void toJson (triagens::basics::Json&, 
                   TRI_memory_zone_t*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the aggregation method from a string
////////////////////////////////////////////////////////////////////////////////
          
      static CollectMethod methodFromString (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the aggregation method
////////////////////////////////////////////////////////////////////////////////
          
      static std::string methodToString (CollectOptions::CollectMethod method);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      CollectMethod method;

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

