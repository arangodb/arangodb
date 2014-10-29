////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, AST variable
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

#ifndef ARANGODB_AQL_VARIABLE_H
#define ARANGODB_AQL_VARIABLE_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Aql/types.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                   struct Variable
// -----------------------------------------------------------------------------

    struct Variable {

////////////////////////////////////////////////////////////////////////////////
/// @brief create the variable
////////////////////////////////////////////////////////////////////////////////

      Variable (std::string const&,
                VariableId);

      Variable (basics::Json const& json);

      Variable *clone() const {
        return new Variable(name, id);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the variable
////////////////////////////////////////////////////////////////////////////////

      ~Variable ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a constant value for the variable
/// this constant value is used for constant propagation in optimizations
////////////////////////////////////////////////////////////////////////////////

      void constValue (void* node) {
        value = node;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a constant value registered for this variable
////////////////////////////////////////////////////////////////////////////////

      inline void* constValue () const {
        return value;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the variable is user-defined
////////////////////////////////////////////////////////////////////////////////

      inline bool isUserDefined () const {
        char const c = name[0];
        // variables starting with a number are not user-defined
        return (c < '0' || c > '9');
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the variable needs a register assigned
////////////////////////////////////////////////////////////////////////////////

      inline bool needsRegister () const {
        char const cf = name[0];
        char const cb = name.back();
        // variables starting with a number are not user-defined
        return (cf < '0' || cf > '9') || cb != '_';
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the variable
////////////////////////////////////////////////////////////////////////////////

      triagens::basics::Json toJson () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a variable by another
////////////////////////////////////////////////////////////////////////////////

      static Variable const* replace (Variable const*,
                                      std::unordered_map<VariableId, Variable const*> const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief variable name
////////////////////////////////////////////////////////////////////////////////

      std::string const   name;

////////////////////////////////////////////////////////////////////////////////
/// @brief constant variable value (points to another AstNode)
////////////////////////////////////////////////////////////////////////////////
      
      void*               value;

////////////////////////////////////////////////////////////////////////////////
/// @brief variable id
////////////////////////////////////////////////////////////////////////////////

      VariableId const    id;

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
