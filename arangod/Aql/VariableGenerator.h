////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, variable generator
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

#ifndef ARANGODB_AQL_VARIABLE_GENERATOR_H
#define ARANGODB_AQL_VARIABLE_GENERATOR_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Aql/Variable.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                           class VariableGenerator
// -----------------------------------------------------------------------------

    class VariableGenerator {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the generator
////////////////////////////////////////////////////////////////////////////////

        VariableGenerator ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the generator
////////////////////////////////////////////////////////////////////////////////

        ~VariableGenerator ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return a map of all variable ids with their names
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<VariableId, std::string const> variables (bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable
////////////////////////////////////////////////////////////////////////////////

        Variable* createVariable (char const*,
                                  bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable
////////////////////////////////////////////////////////////////////////////////

        Variable* createVariable (std::string const&,
                                  bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a variable from JSON
////////////////////////////////////////////////////////////////////////////////

        Variable* createVariable (triagens::basics::Json const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a temporary variable
////////////////////////////////////////////////////////////////////////////////
  
        Variable* createTemporaryVariable ();

////////////////////////////////////////////////////////////////////////////////
/// @brief return a variable by id - this does not respect the scopes!
////////////////////////////////////////////////////////////////////////////////

        Variable* getVariable (VariableId) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next temporary variable name
////////////////////////////////////////////////////////////////////////////////
  
        std::string nextName () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next variable id
////////////////////////////////////////////////////////////////////////////////

        inline VariableId nextId () {
          return _id++;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief all variables created
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<VariableId, Variable*>  _variables;

////////////////////////////////////////////////////////////////////////////////
/// @brief the next assigned variable id
////////////////////////////////////////////////////////////////////////////////

        VariableId                                 _id;

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
