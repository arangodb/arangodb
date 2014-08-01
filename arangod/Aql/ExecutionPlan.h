////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, execution plan
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

#ifndef ARANGODB_AQL_EXECUTION_PLAN_H
#define ARANGODB_AQL_EXECUTION_PLAN_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace aql {

    class Ast;
    struct AstNode;
    class CalculationNode;
    class ExecutionNode;

// -----------------------------------------------------------------------------
// --SECTION--                                               class ExecutionPlan
// -----------------------------------------------------------------------------

    class ExecutionPlan {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the plan
////////////////////////////////////////////////////////////////////////////////

        ExecutionPlan ();

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the plan, frees all assigned nodes
////////////////////////////////////////////////////////////////////////////////

        ~ExecutionPlan ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from an AST
////////////////////////////////////////////////////////////////////////////////

        static ExecutionPlan* instanciateFromAst (Ast const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the root node
////////////////////////////////////////////////////////////////////////////////
        
        ExecutionNode* root () const {
          TRI_ASSERT(_root != nullptr);
          return _root;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief add a node to the plan
////////////////////////////////////////////////////////////////////////////////

        void addNode (ExecutionNode*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a calculation node for an arbitrary expression
////////////////////////////////////////////////////////////////////////////////

        CalculationNode* createTemporaryCalculation (Ast const*,
                                                     AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds "previous" as dependency to "plan", returns "plan"
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* addDependency (ExecutionNode*,
                                      ExecutionNode*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST FOR node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeFor (Ast const*,
                                    ExecutionNode*,
                                    AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST FILTER node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeFilter (Ast const*,
                                       ExecutionNode*,
                                       AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST LET node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeLet (Ast const*,
                                    ExecutionNode*,
                                    AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST SORT node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeSort (Ast const*,
                                     ExecutionNode*,
                                     AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST COLLECT node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeCollect (Ast const*,
                                        ExecutionNode*,
                                        AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST LIMIT node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeLimit (Ast const*,
                                      ExecutionNode*,
                                      AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST RETURN node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeReturn (Ast const*,
                                       ExecutionNode*,
                                       AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST REMOVE node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeRemove (Ast const*,
                                       ExecutionNode*,
                                       AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST INSERT node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeInsert (Ast const*,
                                       ExecutionNode*,
                                       AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST UPDATE node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeUpdate (Ast const*,
                                       ExecutionNode*,
                                       AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan element from an AST REPLACE node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* fromNodeReplace (Ast const*,
                                        ExecutionNode*,
                                        AstNode const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an execution plan from an abstract syntax tree node
////////////////////////////////////////////////////////////////////////////////
  
        ExecutionNode* fromNode (Ast const*,
                                 AstNode const*);


// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief all nodes registered, used for memory management
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionNode*>  _nodes;

////////////////////////////////////////////////////////////////////////////////
/// @brief root node of the engine
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode*               _root;

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
