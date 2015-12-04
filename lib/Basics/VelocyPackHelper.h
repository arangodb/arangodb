////////////////////////////////////////////////////////////////////////////////
/// @brief velocypack helper functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_VELOCYPACK_HELPER_H
#define ARANGODB_BASICS_VELOCYPACK_HELPER_H 1

#include "Basics/JsonHelper.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                            class VelocyPackHelper
// -----------------------------------------------------------------------------

    class VelocyPackHelper {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      private:

        VelocyPackHelper () = delete;
        ~VelocyPackHelper () = delete;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric value
////////////////////////////////////////////////////////////////////////////////

        template<typename T> 
        static T getNumericValue (VPackSlice const& slice,
                                  T defaultValue) {
          if (slice.isNumber()) {
            return slice.getNumber<T>();
          }
          return defaultValue;
        }
 
////////////////////////////////////////////////////////////////////////////////
/// @brief returns a numeric sub-element, or a default if it does not exist
////////////////////////////////////////////////////////////////////////////////

        template<typename T> 
        static T getNumericValue (VPackSlice const& slice,
                                  char const* name,
                                  T defaultValue) {
          VPackSlice sub = slice.get(name);
          if (sub.isNumber()) {
            return sub.getNumber<T>();
          }
          return defaultValue;
        }
 

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a boolean sub-element, or a default if it does not exist
////////////////////////////////////////////////////////////////////////////////

        static bool getBooleanValue (VPackSlice const&,
                                     char const*,
                                     bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a string sub-element, or throws if <name> does not exist
/// or it is not a string 
////////////////////////////////////////////////////////////////////////////////

        static std::string checkAndGetStringValue (VPackSlice const&,
                                                   char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Build TRI_json_t from VelocyPack. Just a temporary solution
////////////////////////////////////////////////////////////////////////////////

        static TRI_json_t* velocyPackToJson (VPackSlice const&);
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
