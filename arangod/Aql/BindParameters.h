////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, bind parameters
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_QUERY_BIND_PARAMETERS_H
#define ARANGODB_AQL_QUERY_BIND_PARAMETERS_H 1

#include "Basics/Common.h"
#include "Basics/json.h"

namespace triagens {
  namespace aql {

    typedef std::unordered_map<std::string, std::pair<TRI_json_t const*, bool>> BindParametersType;

// -----------------------------------------------------------------------------
// --SECTION--                                              class BindParameters
// -----------------------------------------------------------------------------

    class BindParameters {
      
// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the parameters
////////////////////////////////////////////////////////////////////////////////

        BindParameters (TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the parameters
////////////////////////////////////////////////////////////////////////////////

        ~BindParameters ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return all parameters
////////////////////////////////////////////////////////////////////////////////

        BindParametersType const& operator() () {
          process();
          return _parameters;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief process the parameters
////////////////////////////////////////////////////////////////////////////////

        void process ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the parameter json
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t*  _json;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to collection parameters
////////////////////////////////////////////////////////////////////////////////

        BindParametersType _parameters;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal state
////////////////////////////////////////////////////////////////////////////////

        bool _processed;

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
