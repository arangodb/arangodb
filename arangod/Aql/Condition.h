////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, condition 
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

#ifndef ARANGODB_AQL_CONDITION_H
#define ARANGODB_AQL_CONDITION_H 1

#include "Basics/Common.h"
#include "Aql/AstNode.h"

namespace triagens {
  namespace aql {

    class Ast;
    struct Variable;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief side on which an attribute occurs in a condition
////////////////////////////////////////////////////////////////////////////////

    enum AttributeSideType {
      ATTRIBUTE_LEFT,
      ATTRIBUTE_RIGHT
    };

// -----------------------------------------------------------------------------
// --SECTION--                                              struct ConditionPart
// -----------------------------------------------------------------------------

    struct ConditionPart {

      enum ConditionPartCompareResult {
        IMPOSSIBLE,
        SELF_CONTAINED_IN_OTHER,
        OTHER_CONTAINED_IN_SELF,
        DISJOINT,
        UNKNOWN
      };

      ConditionPart () = delete;

      ConditionPart (Variable const*,
                     std::string const&,
                     size_t,
                     AstNode const*,
                     AttributeSideType);

      ~ConditionPart ();

      ConditionPartCompareResult compare (ConditionPart const&) const;

      void dump () const;
      
      Variable const*   variable;
      std::string const attributeName;
      size_t            sourcePosition;
      AstNodeType       operatorType;
      AstNode const*    operatorNode;
      AstNode const*    valueNode;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                   class Condition
// -----------------------------------------------------------------------------

    class Condition {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        Condition (Condition const&) = delete;
        Condition& operator= (Condition const&) = delete;
        Condition () = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create the condition
////////////////////////////////////////////////////////////////////////////////

        explicit Condition (Ast*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the condition
////////////////////////////////////////////////////////////////////////////////

        ~Condition ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief add a sub-condition to the condition
/// the sub-condition will be AND-combined with the existing condition(s)
////////////////////////////////////////////////////////////////////////////////

        void andCombine (AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize the condition
/// this will convert the condition into its disjunctive normal form
////////////////////////////////////////////////////////////////////////////////

        void normalize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize the condition expression tree
////////////////////////////////////////////////////////////////////////////////

        void optimize ();

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the condition
////////////////////////////////////////////////////////////////////////////////

        void dump () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the AST, used for memory management
////////////////////////////////////////////////////////////////////////////////

        Ast*      _ast;

////////////////////////////////////////////////////////////////////////////////
/// @brief root node of the condition
////////////////////////////////////////////////////////////////////////////////

        AstNode*  _root;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the condition was already normalized
////////////////////////////////////////////////////////////////////////////////

        bool _isNormalized;

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
