////////////////////////////////////////////////////////////////////////////////
/// @brief AST optimization functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_QL_OPTIMIZE
#define TRIAGENS_DURHAM_QL_OPTIMIZE

#include <BasicsC/conversions.h>

#include "QL/ast-node.h"
#include "QL/ast-query.h"


#ifdef __cplusplus
extern "C" {
#endif


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as an arithmetic operand
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeCanBeUsedAsArithmeticOperand (const QL_ast_node_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as a relational operand
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeCanBeUsedAsRelationalOperand (const QL_ast_node_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a node is optimizable as a logical operand
////////////////////////////////////////////////////////////////////////////////

bool QLOptimizeCanBeUsedAsLogicalOperand (const QL_ast_node_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for unary operators
///
/// this function will optimize unary plus and unary minus operators that are
/// used together with constant values, e.g. it will merge the two nodes "-" and
/// "1" to a "-1".
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeUnaryOperator (QL_ast_node_t *);


////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for binary operators
///
/// this function will optimize arithmetic, relational and logical operators 
/// that are used together with constant values. It will replace the binary 
/// operator with the result of the optimization. 
/// The node will therefore change its type from a binary operator to a value
/// type node. The former sub nodes (lhs and rhs) of the binary operator will 
/// be unlinked, but not be freed here.
/// Freeing memory is done later when the whole query structure is deallocated.
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeBinaryOperator (QL_ast_node_t *);


////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for the ternary operator
///
/// this function will optimize the ternary operator if the conditional part is
/// reducible to a constant. It will substitute the condition with the true 
/// part if the condition is true, and with the false part if the condition is
/// false.
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeTernaryOperator (QL_ast_node_t *);


////////////////////////////////////////////////////////////////////////////////
/// @brief optimize order by
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeOrder (QL_ast_node_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief recursively optimize nodes in an AST expression
///
/// this function will walk the AST recursively and will start optimization from
/// the bottom-up.
/// TODO: it should be changed to a non-recursive function to allow arbibrary
/// nesting levels
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeExpression (QL_ast_node_t *);


////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's SELECT part
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_select_type_e QLOptimizeGetSelectType (const QL_ast_query_t*);


////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's WHERE condition
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_where_type_e QLOptimizeGetWhereType (const QL_ast_query_t*);


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

