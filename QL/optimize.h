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
#include <BasicsC/hashes.h>
#include <BasicsC/vector.h>
#include <BasicsC/strings.h>
#include <BasicsC/string-buffer.h>

#include "QL/ast-node.h"
#include "QL/ast-query.h"
#include "QL/parser-context.h"
#include "QL/formatter.h"
#include "QL/javascripter.h"
#include "VocBase/index.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page Optimizer Query optimizer
/// 
/// The AQL query optimizer inspects incoming select queries and applies simple
/// transformations to optimize them. The goal of all optimization is to do
/// less work when executing the query and produce the results faster.
///
/// @section Optimizer transformations
/// 
/// Currently, the AQL query optimizer applies the following transformations:
/// - constant folding: numeric literals, boolean values and null are folded 
///   if possible in the select part, on clause, where clause and order by parts
///   of queries. Constant folding is applied for arithmetic and logical 
///   operators if both operands are constants.
///   Furthermore, constant string literal comparisons are evaluated during
///   parsing and replaced with their results. Idem comparisons of collection
///   attributes (e.g. @LIT{users.id == users.id} or @LIT{locs.lat != locs.lat})
///   are also replaced with the boolean result of the comparison.
/// - where clause removal: if the where clause evaluates to a constant value, 
///   it is removed completely. If this constant always evaluates to false, the 
///   entire query execution is skipped because the result would be empty anyway.
/// - order by removal: all order by expressions in a query that evaluate to a
///   constant value are removed as they would not influence the sorting. This 
///   might lead to all order by parts being removed.
/// - join removal: if a collection is left join'd, right join'd, or list join'd
///   but is not referenced anywhere in the select, where, or order by clauses,
///   it will not influence the results produced by the query. There is no need 
///   to execute the join on the collection and thus the join is removed.
/// - range optimization: invalid value ranges in the where clause are detected
///   for fields if the values are numbers or strings and the operators equality,
///   greater/greater than, less/less than are used, and the range conditions
///   are combined with logical ands. For example, the following range condition
///   will be detected as being impossible and removed: 
///   @LIT{users.id > 3 && users.id < 3}
///
/// @section Optimizer issues
///
/// The optimizer currently cannot optimize subconditions combined with a 
/// logical @LIT{||}. Furthermore, it will not optimize negated conditions or
/// subconditions. It can only combine multiple subconditions on the same 
/// attribute if the compare values have the same type (numeric or string).
/// The optimizer will not merge conditions from the where clause and any of the
/// on clauses although this might be theoretically possible in some cases.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup QL
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Range status types 
///
/// The range status values are used to describe the lower and upper bounds 
/// values of a @ref QL_optimize_range_t. The status values have the following 
/// meanings:
/// - RANGE_VALUE_INFINITE: indicates that the value is unbounded (infinity)
/// - RANGE_VALUE_INCLUDED: indicates that the value is included in the range
/// - RANGE_VALUE_EXCLUDED: indicates that the value is not included in the range
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  RANGE_VALUE_INFINITE  = 1,
  RANGE_VALUE_INCLUDED  = 2,
  RANGE_VALUE_EXCLUDED  = 3
}
QL_optimize_range_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief Range value types 
///
/// Currently supported types are collection attributes (fields), doubles 
/// (numbers), strings, and JSON documents.
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  RANGE_TYPE_FIELD  = 1,
  RANGE_TYPE_DOUBLE = 2,
  RANGE_TYPE_STRING = 3,
  RANGE_TYPE_JSON   = 4
}
QL_optimize_range_value_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief Range type
///
/// Ranges are used to store the value ranges that are requested in a query
/// condition part. For example, if the where condition is @LIT{users.id > 1},
/// a range for users.id from [ 1 ... +inf] would be created. If further 
/// conditions are found, additional ranges will be created for them. Multiple
/// ranges combined by logical && and || operators will then be merged into
/// combined ranges. 
///
/// This might already remove some illogical ranges (e.g. for the condition
/// @LIT{users.id == 1 && users.id == 2}) or simplify multiple conditions into
/// one combined condition (e.g. @LIT{users.id > 3 && users.id > 4} would be
/// merged into @LIT{users.id > 4}.
///
/// For each range, the data type (double or string) is stored. Only ranges of
/// the same data type can be combined together. Ranges with different types are
/// not optimized. Each range has a minimum and a maximum value (bounds), both 
/// of which might be infinite in the case of range conditions or for 
/// unrestricted ranges.
///
/// Infinity is indicated by _minStatus or _maxStatus set to 
/// (RANGE_VALUE_INFINITE). If the value is not infinity, the status 
/// indicates whether the bounds value is included (RANGE_VALUE_INCLUDED) 
/// or not included (RANGE_VALUE_EXCLUDED) in the range.
////////////////////////////////////////////////////////////////////////////////

typedef struct QL_optimize_range_s {
  char* _collection;
  char* _field;
  QL_optimize_range_value_type_e _valueType;
  union {
    double _doubleValue;
    char* _stringValue;
  } _minValue;
  union {
    double _doubleValue;
    char* _stringValue;
  } _maxValue;
  struct {
    char* _collection;
    char* _field;
  } _refValue;
  uint64_t _hash;
  QL_optimize_range_type_e _minStatus;
  QL_optimize_range_type_e _maxStatus;
}
QL_optimize_range_t;

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

void QLOptimizeUnaryOperator (QL_ast_node_t*);

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

void QLOptimizeBinaryOperator (QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief optimization function for the ternary operator
///
/// this function will optimize the ternary operator if the conditional part is
/// reducible to a constant. It will substitute the condition with the true 
/// part if the condition is true, and with the false part if the condition is
/// false.
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeTernaryOperator (QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize order by by removing constant parts
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

void QLOptimizeExpression (QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief optimize from/joins
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeFrom (const QL_parser_context_t* context);

////////////////////////////////////////////////////////////////////////////////
/// @brief Free all existing ranges in a range vector
////////////////////////////////////////////////////////////////////////////////

void QLOptimizeFreeRangeVector (TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively optimize condition expressions
///
/// this function will walk the AST recursively and will start optimization from
/// the bottom-up. this function is suited for conditional expressions as it
/// tries to find suitable ranges for index accesses etc.
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* QLOptimizeCondition (QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's SELECT part
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_select_type_e QLOptimizeGetSelectType (const QL_ast_query_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's WHERE/ON condition
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_where_type_e QLOptimizeGetWhereType (const QL_ast_node_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type of a query's ORDER BY condition
////////////////////////////////////////////////////////////////////////////////

QL_ast_query_order_type_e QLOptimizeGetOrderType (const QL_ast_node_t*);

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

