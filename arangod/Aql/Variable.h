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

namespace triagens {
  namespace aql {

    typedef uint32_t VariableId; 

// -----------------------------------------------------------------------------
// --SECTION--                                                   struct Variable
// -----------------------------------------------------------------------------

    struct Variable {

////////////////////////////////////////////////////////////////////////////////
/// @brief create the variable
////////////////////////////////////////////////////////////////////////////////

      Variable (std::string const&,
                VariableId);

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
/// @brief whether or not the variable is reference counted
////////////////////////////////////////////////////////////////////////////////

      inline bool isReferenceCounted () const {
        return (refCount > 0);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the variable's reference counter
////////////////////////////////////////////////////////////////////////////////

      inline void increaseReferenceCount () {
        ++refCount;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the variable's reference counter
////////////////////////////////////////////////////////////////////////////////

      inline void decreaseReferenceCount () {
        TRI_ASSERT(refCount > 0);
        --refCount;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the variable is user-defined
////////////////////////////////////////////////////////////////////////////////

      inline bool isUserDefined () const {
        char const c = name[0];
        // variables starting with a number are user-defined
        return (c >= '0' && c <= '9');
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the variable
////////////////////////////////////////////////////////////////////////////////

      triagens::basics::Json toJson () const;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief number of times the variable is used in the AST
////////////////////////////////////////////////////////////////////////////////

      uint32_t            refCount;

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
