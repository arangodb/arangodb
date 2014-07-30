////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, AST variables
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

namespace triagens {
  namespace aql {

    typedef uint32_t VariableId; 

// -----------------------------------------------------------------------------
// --SECTION--                                                   struct Variable
// -----------------------------------------------------------------------------

    struct Variable {
      Variable (std::string const& name,
                VariableId id,
                bool isUserDefined)
        : name(name),
          value(nullptr),
          id(id),
          refCount(0),
          isUserDefined(isUserDefined) {
      }

      ~Variable () {
      }

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      std::string const  name;
      void*              value;
      VariableId const   id;
      uint32_t           refCount;
      bool const         isUserDefined;

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
