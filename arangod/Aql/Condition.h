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
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/JsonHelper.h"

namespace triagens {
  namespace aql {

    class Ast;
    class EnumerateCollectionNode;
    class ExecutionPlan;
    struct Index;
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
        IMPOSSIBLE              = 0,
        SELF_CONTAINED_IN_OTHER = 1,
        OTHER_CONTAINED_IN_SELF = 2,
        DISJOINT                = 3,
        CONVERT_EQUAL           = 4,
        UNKNOWN                 = 5
      };
      static ConditionPartCompareResult const ResultsTable[3][7][7];

      ConditionPart () = delete;

      ConditionPart (Variable const*,
                     std::string const&,
                     size_t,
                     AstNode const*,
                     AttributeSideType);

      ~ConditionPart ();

      inline uint whichCompareOperation() const {
        switch (operatorType) {
        case NODE_TYPE_OPERATOR_BINARY_EQ:
          return 0;
        case NODE_TYPE_OPERATOR_BINARY_NE:
          return 1;
        case NODE_TYPE_OPERATOR_BINARY_LT:
          return 2;
        case NODE_TYPE_OPERATOR_BINARY_LE:
          return 3;
        case NODE_TYPE_OPERATOR_BINARY_GE:
          return 4;
        case NODE_TYPE_OPERATOR_BINARY_GT:
          return 5;
        default:
          return 6; // not a compare operator.
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the condition for debug purposes
////////////////////////////////////////////////////////////////////////////////

      void dump () const;
      
      Variable const*             variable;
      std::string const           attributeName;
      size_t                      sourcePosition;
      AstNodeType                 operatorType;
      AstNode const*              operatorNode;
      AstNode const*              valueNode;
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
/// @brief return the condition root
////////////////////////////////////////////////////////////////////////////////

        inline AstNode const* root () const {
          return _root;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the condition as a Json object
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json toJson (TRI_memory_zone_t* zone) const {
          if (_root == nullptr) {
            return triagens::basics::Json(triagens::basics::Json::Object);
          }

          return triagens::basics::Json(zone, _root->toJson(zone, false));
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a sub-condition to the condition
/// the sub-condition will be AND-combined with the existing condition(s)
////////////////////////////////////////////////////////////////////////////////

        void andCombine (AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize the condition
/// this will convert the condition into its disjunctive normal form
////////////////////////////////////////////////////////////////////////////////

        void normalize (ExecutionPlan* plan);

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize the condition expression tree
////////////////////////////////////////////////////////////////////////////////

        void optimize (ExecutionPlan* plan);

////////////////////////////////////////////////////////////////////////////////
/// @brief locate indexes which can be used for conditions
////////////////////////////////////////////////////////////////////////////////

        bool findIndexes (EnumerateCollectionNode const*, 
                          std::vector<Index const*>&, 
                          SortCondition const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the condition
////////////////////////////////////////////////////////////////////////////////

        void dump () const;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief deduplicate IN condition values
/// this may modify the node in place
////////////////////////////////////////////////////////////////////////////////

        void deduplicateInValues (AstNode*);

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms the AstNode
////////////////////////////////////////////////////////////////////////////////

        AstNode* transformNode (AstNode*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Collapses nested logical AND/OR nodes
////////////////////////////////////////////////////////////////////////////////

        AstNode* collapseNesting (AstNode*);

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a top-level OR node if it does not already exist, and make sure that all second
///        level nodes are AND nodes. Additionally, this processing step will
///        remove all NOP nodes.
////////////////////////////////////////////////////////////////////////////////

        AstNode* fixRoot (AstNode*, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds the best index that can match this single node
////////////////////////////////////////////////////////////////////////////////

        bool findIndexForAndNode (AstNode const*, 
                                  Variable const*, 
                                  EnumerateCollectionNode const*, 
                                  std::vector<Index const*>&,
                                  SortCondition const&);

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
