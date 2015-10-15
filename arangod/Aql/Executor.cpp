////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, expression executor
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

#include "Aql/Executor.h"
#include "Aql/AstNode.h"
#include "Aql/Functions.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/StringBuffer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8Server/v8-shape-conv.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             static initialization
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if code is executed in cluster or not
////////////////////////////////////////////////////////////////////////////////

static ExecutionCondition const NotInCluster = [] {
  return ! triagens::arango::ServerState::instance()->isRunningInCluster();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal functions used in execution
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<int, std::string const> const Executor::InternalFunctionNames{ 
  { static_cast<int>(NODE_TYPE_OPERATOR_UNARY_PLUS),   "UNARY_PLUS" },
  { static_cast<int>(NODE_TYPE_OPERATOR_UNARY_MINUS),  "UNARY_MINUS" },
  { static_cast<int>(NODE_TYPE_OPERATOR_UNARY_NOT),    "LOGICAL_NOT" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_EQ),    "RELATIONAL_EQUAL" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NE),    "RELATIONAL_UNEQUAL" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GT),    "RELATIONAL_GREATER" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GE),    "RELATIONAL_GREATEREQUAL" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LT),    "RELATIONAL_LESS" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LE),    "RELATIONAL_LESSEQUAL" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_IN),    "RELATIONAL_IN" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NIN),   "RELATIONAL_NOT_IN" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_PLUS),  "ARITHMETIC_PLUS" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MINUS), "ARITHMETIC_MINUS" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_TIMES), "ARITHMETIC_TIMES" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_DIV),   "ARITHMETIC_DIVIDE" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MOD),   "ARITHMETIC_MODULUS" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_AND),   "LOGICAL_AND" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_OR),    "LOGICAL_OR" },
  { static_cast<int>(NODE_TYPE_OPERATOR_TERNARY),      "TERNARY_OPERATOR" }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief user-accessible functions
////////////////////////////////////////////////////////////////////////////////

std::unordered_map<std::string, Function const> const Executor::FunctionNames{ 
  // meanings of the symbols in the function arguments list
  // ------------------------------------------------------
  //
  // . = argument of any type (except collection)
  // c = collection name, will be converted into list with documents
  // h = collection name, will be converted into string
  // z = null
  // b = bool
  // n = number
  // s = string
  // p = primitive
  // l = array
  // a = object / document
  // r = regex (a string with a special format). note: the regex type is mutually exclusive with all other types

  // type check functions
  { "IS_NULL",                     Function("IS_NULL",                     "AQL_IS_NULL", ".", true, true, false, true, true, &Functions::IsNull) },
  { "IS_BOOL",                     Function("IS_BOOL",                     "AQL_IS_BOOL", ".", true, true, false, true, true, &Functions::IsBool) },
  { "IS_NUMBER",                   Function("IS_NUMBER",                   "AQL_IS_NUMBER", ".", true, true, false, true, true, &Functions::IsNumber) },
  { "IS_STRING",                   Function("IS_STRING",                   "AQL_IS_STRING", ".", true, true, false, true, true, &Functions::IsString) },
  { "IS_ARRAY",                    Function("IS_ARRAY",                    "AQL_IS_ARRAY", ".", true, true, false, true, true, &Functions::IsArray) },
  // IS_LIST is an alias for IS_ARRAY
  { "IS_LIST",                     Function("IS_LIST",                     "AQL_IS_LIST", ".", true, true, false, true, true, &Functions::IsArray) },
  { "IS_OBJECT",                   Function("IS_OBJECT",                   "AQL_IS_OBJECT", ".", true, true, false, true, true, &Functions::IsObject) },
  // IS_DOCUMENT is an alias for IS_OBJECT
  { "IS_DOCUMENT",                 Function("IS_DOCUMENT",                 "AQL_IS_DOCUMENT", ".", true, true, false, true, true, &Functions::IsObject) }, 
  
  // type cast functions
  { "TO_NUMBER",                   Function("TO_NUMBER",                   "AQL_TO_NUMBER", ".", true, true, false, true, true, &Functions::ToNumber) },
  { "TO_STRING",                   Function("TO_STRING",                   "AQL_TO_STRING", ".", true, true, false, true, true, &Functions::ToString) },
  { "TO_BOOL",                     Function("TO_BOOL",                     "AQL_TO_BOOL", ".", true, true, false, true, true, &Functions::ToBool) },
  { "TO_ARRAY",                    Function("TO_ARRAY",                    "AQL_TO_ARRAY", ".", true, true, false, true, true, &Functions::ToArray) },
  // TO_LIST is an alias for TO_ARRAY
  { "TO_LIST",                     Function("TO_LIST",                     "AQL_TO_LIST", ".", true, true, false, true, true, &Functions::ToArray) },
  
  // string functions
  { "CONCAT",                      Function("CONCAT",                      "AQL_CONCAT", "szl|+", true, true, false, true, true, &Functions::Concat) },
  { "CONCAT_SEPARATOR",            Function("CONCAT_SEPARATOR",            "AQL_CONCAT_SEPARATOR", "s,szl|+", true, true, false, true, true) },
  { "CHAR_LENGTH",                 Function("CHAR_LENGTH",                 "AQL_CHAR_LENGTH", "s", true, true, false, true, true) },
  { "LOWER",                       Function("LOWER",                       "AQL_LOWER", "s", true, true, false, true, true) },
  { "UPPER",                       Function("UPPER",                       "AQL_UPPER", "s", true, true, false, true, true) },
  { "SUBSTRING",                   Function("SUBSTRING",                   "AQL_SUBSTRING", "s,n|n", true, true, false, true, true) },
  { "CONTAINS",                    Function("CONTAINS",                    "AQL_CONTAINS", "s,s|b", true, true, false, true, true) },
  { "LIKE",                        Function("LIKE",                        "AQL_LIKE", "s,r|b", true, true, false, true, true, &Functions::Like) },
  { "LEFT",                        Function("LEFT",                        "AQL_LEFT", "s,n", true, true, false, true, true) },
  { "RIGHT",                       Function("RIGHT",                       "AQL_RIGHT", "s,n", true, true, false, true, true) },
  { "TRIM",                        Function("TRIM",                        "AQL_TRIM", "s|ns", true, true, false, true, true) },
  { "LTRIM",                       Function("LTRIM",                       "AQL_LTRIM", "s|s", true, true, false, true, true) },
  { "RTRIM",                       Function("RTRIM",                       "AQL_RTRIM", "s|s", true, true, false, true, true) },
  { "FIND_FIRST",                  Function("FIND_FIRST",                  "AQL_FIND_FIRST", "s,s|zn,zn", true, true, false, true, true) },
  { "FIND_LAST",                   Function("FIND_LAST",                   "AQL_FIND_LAST", "s,s|zn,zn", true, true, false, true, true) },
  { "SPLIT",                       Function("SPLIT",                       "AQL_SPLIT", "s|sl,n", true, true, false, true, true) },
  { "SUBSTITUTE",                  Function("SUBSTITUTE",                  "AQL_SUBSTITUTE", "s,las|lsn,n", true, true, false, true, true) },
  { "MD5",                         Function("MD5",                         "AQL_MD5", "s", true, true, false, true, true, &Functions::Md5) },
  { "SHA1",                        Function("SHA1",                        "AQL_SHA1", "s", true, true, false, true, true, &Functions::Sha1) },
  { "RANDOM_TOKEN",                Function("RANDOM_TOKEN",                "AQL_RANDOM_TOKEN", "n", false, false, true, true, true) },

  // numeric functions
  { "FLOOR",                       Function("FLOOR",                       "AQL_FLOOR", "n", true, true, false, true, true) },
  { "CEIL",                        Function("CEIL",                        "AQL_CEIL", "n", true, true, false, true, true) },
  { "ROUND",                       Function("ROUND",                       "AQL_ROUND", "n", true, true, false, true, true) },
  { "ABS",                         Function("ABS",                         "AQL_ABS", "n", true, true, false, true, true) },
  { "RAND",                        Function("RAND",                        "AQL_RAND", "", false, false, false, true, true) },
  { "SQRT",                        Function("SQRT",                        "AQL_SQRT", "n", true, true, false, true, true) },
  
  // list functions
  { "RANGE",                       Function("RANGE",                       "AQL_RANGE", "n,n|n", true, true, false, true, true) },
  { "UNION",                       Function("UNION",                       "AQL_UNION", "l,l|+", true, true, false, true, true, &Functions::Union) },
  { "UNION_DISTINCT",              Function("UNION_DISTINCT",              "AQL_UNION_DISTINCT", "l,l|+", true, true, false, true, true, &Functions::UnionDistinct) },
  { "MINUS",                       Function("MINUS",                       "AQL_MINUS", "l,l|+", true, true, false, true, true) },
  { "INTERSECTION",                Function("INTERSECTION",                "AQL_INTERSECTION", "l,l|+", true, true, false, true, true, &Functions::Intersection) },
  { "FLATTEN",                     Function("FLATTEN",                     "AQL_FLATTEN", "l|n", true, true, false, true, true) },
  { "LENGTH",                      Function("LENGTH",                      "AQL_LENGTH", "las", true, true, false, true, true, &Functions::Length) },
  { "MIN",                         Function("MIN",                         "AQL_MIN", "l", true, true, false, true, true, &Functions::Min) },
  { "MAX",                         Function("MAX",                         "AQL_MAX", "l", true, true, false, true, true, &Functions::Max) },
  { "SUM",                         Function("SUM",                         "AQL_SUM", "l", true, true, false, true, true, &Functions::Sum) },
  { "MEDIAN",                      Function("MEDIAN",                      "AQL_MEDIAN", "l", true, true, false, true, true) }, 
  { "PERCENTILE",                  Function("PERCENTILE",                  "AQL_PERCENTILE", "l,n|s", true, true, false, true, true) }, 
  { "AVERAGE",                     Function("AVERAGE",                     "AQL_AVERAGE", "l", true, true, false, true, true, &Functions::Average) },
  { "VARIANCE_SAMPLE",             Function("VARIANCE_SAMPLE",             "AQL_VARIANCE_SAMPLE", "l", true, true, false, true, true) },
  { "VARIANCE_POPULATION",         Function("VARIANCE_POPULATION",         "AQL_VARIANCE_POPULATION", "l", true, true, false, true, true) },
  { "STDDEV_SAMPLE",               Function("STDDEV_SAMPLE",               "AQL_STDDEV_SAMPLE", "l", true, true, false, true, true) },
  { "STDDEV_POPULATION",           Function("STDDEV_POPULATION",           "AQL_STDDEV_POPULATION", "l", true, true, false, true, true) },
  { "UNIQUE",                      Function("UNIQUE",                      "AQL_UNIQUE", "l", true, true, false, true, true, &Functions::Unique) },
  { "SORTED_UNIQUE",               Function("SORTED_UNIQUE",               "AQL_SORTED_UNIQUE", "l", true, true, false, true, true, &Functions::SortedUnique) },
  { "SLICE",                       Function("SLICE",                       "AQL_SLICE", "l,n|n", true, true, false, true, true) },
  { "REVERSE",                     Function("REVERSE",                     "AQL_REVERSE", "ls", true, true, false, true, true) },    // note: REVERSE() can be applied on strings, too
  { "FIRST",                       Function("FIRST",                       "AQL_FIRST", "l", true, true, false, true, true) },
  { "LAST",                        Function("LAST",                        "AQL_LAST", "l", true, true, false, true, true) },
  { "NTH",                         Function("NTH",                         "AQL_NTH", "l,n", true, true, false, true, true) },
  { "POSITION",                    Function("POSITION",                    "AQL_POSITION", "l,.|b", true, true, false, true, true) },
  { "CALL",                        Function("CALL",                        "AQL_CALL", "s|.+", false, false, true, false, true) },
  { "APPLY",                       Function("APPLY",                       "AQL_APPLY", "s|l", false, false, true, false, false) },
  { "PUSH",                        Function("PUSH",                        "AQL_PUSH", "l,.|b", true, true, false, true, false) },
  { "APPEND",                      Function("APPEND",                      "AQL_APPEND", "l,lz|b", true, true, false, true, true) },
  { "POP",                         Function("POP",                         "AQL_POP", "l", true, true, false, true, true) },
  { "SHIFT",                       Function("SHIFT",                       "AQL_SHIFT", "l", true, true, false, true, true) },
  { "UNSHIFT",                     Function("UNSHIFT",                     "AQL_UNSHIFT", "l,.|b", true, true, false, true, true) },
  { "REMOVE_VALUE",                Function("REMOVE_VALUE",                "AQL_REMOVE_VALUE", "l,.|n", true, true, false, true, true) },
  { "REMOVE_VALUES",               Function("REMOVE_VALUES",               "AQL_REMOVE_VALUES", "l,lz", true, true, false, true, true) },
  { "REMOVE_NTH",                  Function("REMOVE_NTH",                  "AQL_REMOVE_NTH", "l,n", true, true, false, true, true) },

  // document functions
  { "HAS",                         Function("HAS",                         "AQL_HAS", "az,s", true, true, false, true, true, &Functions::Has) },
  { "ATTRIBUTES",                  Function("ATTRIBUTES",                  "AQL_ATTRIBUTES", "a|b,b", true, true, false, true, true, &Functions::Attributes) },
  { "VALUES",                      Function("VALUES",                      "AQL_VALUES", "a|b", true, true, false, true, true, &Functions::Values) },
  { "MERGE",                       Function("MERGE",                       "AQL_MERGE", "a,a|+", true, true, false, true, true, &Functions::Merge) },
  { "MERGE_RECURSIVE",             Function("MERGE_RECURSIVE",             "AQL_MERGE_RECURSIVE", "a,a|+", true, true, false, true, true) },
  { "DOCUMENT",                    Function("DOCUMENT",                    "AQL_DOCUMENT", "h.|.", false, false, true, false, true) },
  { "MATCHES",                     Function("MATCHES",                     "AQL_MATCHES", ".,l|b", true, true, false, true, true) },
  { "UNSET",                       Function("UNSET",                       "AQL_UNSET", "a,sl|+", true, true, false, true, true, &Functions::Unset) },
  { "KEEP",                        Function("KEEP",                        "AQL_KEEP", "a,sl|+", true, true, false, true, true, &Functions::Keep) },
  { "TRANSLATE",                   Function("TRANSLATE",                   "AQL_TRANSLATE", ".,a|.", true, true, false, true, true) },
  { "ZIP",                         Function("ZIP",                         "AQL_ZIP", "l,l", true, true, false, true, true) },

  // geo functions
  { "NEAR",                        Function("NEAR",                        "AQL_NEAR", "h,n,n|nz,s", true, false, true, false, true) },
  { "WITHIN",                      Function("WITHIN",                      "AQL_WITHIN", "h,n,n,n|s", true, false, true, false, true) },
  { "WITHIN_RECTANGLE",            Function("WITHIN_RECTANGLE",            "AQL_WITHIN_RECTANGLE", "h,d,d,d,d", true, false, true, false, true) },
  { "IS_IN_POLYGON",               Function("IS_IN_POLYGON",               "AQL_IS_IN_POLYGON", "l,ln|nb", true, true, false, true, true) },

  // fulltext functions
  { "FULLTEXT",                    Function("FULLTEXT",                    "AQL_FULLTEXT", "h,s,s|n", true, false, true, false, true) },

  // graph functions
  { "PATHS",                       Function("PATHS",                       "AQL_PATHS", "c,h|s,ba", true, false, true, false, false) },
  { "GRAPH_PATHS",                 Function("GRAPH_PATHS",                 "AQL_GRAPH_PATHS", "s|a", false, false, true, false, false) },
  { "SHORTEST_PATH",               Function("SHORTEST_PATH",               "AQL_SHORTEST_PATH", "h,h,s,s,s|a", true, false, true, false, false) },
  { "GRAPH_SHORTEST_PATH",         Function("GRAPH_SHORTEST_PATH",         "AQL_GRAPH_SHORTEST_PATH", "s,als,als|a", false, false, true, false, false) },
  { "GRAPH_DISTANCE_TO",           Function("GRAPH_DISTANCE_TO",           "AQL_GRAPH_DISTANCE_TO", "s,als,als|a", false, false, true, false, false) },
  { "TRAVERSAL",                   Function("TRAVERSAL",                   "AQL_TRAVERSAL", "h,h,s,s|a", false, false, true, false, false) },
  { "GRAPH_TRAVERSAL",             Function("GRAPH_TRAVERSAL",             "AQL_GRAPH_TRAVERSAL", "s,als,s|a", false, false, true, false, false) },
  { "TRAVERSAL_TREE",              Function("TRAVERSAL_TREE",              "AQL_TRAVERSAL_TREE", "h,h,s,s,s|a", false, false, true, false, false) },
  { "GRAPH_TRAVERSAL_TREE",        Function("GRAPH_TRAVERSAL_TREE",        "AQL_GRAPH_TRAVERSAL_TREE", "s,als,s,s|a", false, false, true, false, false) },
  { "EDGES",                       Function("EDGES",                       "AQL_EDGES", "h,s,s|l,o", true, false, true, false, false) },
  { "GRAPH_EDGES",                 Function("GRAPH_EDGES",                 "AQL_GRAPH_EDGES", "s,als|a", false, false, true, false, false) },
  { "GRAPH_VERTICES",              Function("GRAPH_VERTICES",              "AQL_GRAPH_VERTICES", "s,als|a", false, false, true, false, false) },
  { "NEIGHBORS",                   Function("NEIGHBORS",                   "AQL_NEIGHBORS", "h,h,s,s|l,a", true, false, true, false, false, &Functions::Neighbors, NotInCluster) },
  { "GRAPH_NEIGHBORS",             Function("GRAPH_NEIGHBORS",             "AQL_GRAPH_NEIGHBORS", "s,als|a", false, false, true, false, false) },
  { "GRAPH_COMMON_NEIGHBORS",      Function("GRAPH_COMMON_NEIGHBORS",      "AQL_GRAPH_COMMON_NEIGHBORS", "s,als,als|a,a", false, false, true, false, false) },
  { "GRAPH_COMMON_PROPERTIES",     Function("GRAPH_COMMON_PROPERTIES",     "AQL_GRAPH_COMMON_PROPERTIES", "s,als,als|a", false, false, true, false, false) },
  { "GRAPH_ECCENTRICITY",          Function("GRAPH_ECCENTRICITY",          "AQL_GRAPH_ECCENTRICITY", "s|a", false, false, true, false, false) },
  { "GRAPH_BETWEENNESS",           Function("GRAPH_BETWEENNESS",           "AQL_GRAPH_BETWEENNESS", "s|a", false, false, true, false, false) },
  { "GRAPH_CLOSENESS",             Function("GRAPH_CLOSENESS",             "AQL_GRAPH_CLOSENESS", "s|a", false, false, true, false, false) },
  { "GRAPH_ABSOLUTE_ECCENTRICITY", Function("GRAPH_ABSOLUTE_ECCENTRICITY", "AQL_GRAPH_ABSOLUTE_ECCENTRICITY", "s,als|a", false, false, true, false, false) },
  { "GRAPH_ABSOLUTE_BETWEENNESS",  Function("GRAPH_ABSOLUTE_BETWEENNESS",  "AQL_GRAPH_ABSOLUTE_BETWEENNESS", "s,als|a", false, false, true, false, false) },
  { "GRAPH_ABSOLUTE_CLOSENESS",    Function("GRAPH_ABSOLUTE_CLOSENESS",    "AQL_GRAPH_ABSOLUTE_CLOSENESS", "s,als|a", false, false, true, false, false) },
  { "GRAPH_DIAMETER",              Function("GRAPH_DIAMETER",              "AQL_GRAPH_DIAMETER", "s|a", false, false, true, false, false) },
  { "GRAPH_RADIUS",                Function("GRAPH_RADIUS",                "AQL_GRAPH_RADIUS", "s|a", false, false, true, false, false) },

  // date functions
  { "DATE_NOW",                    Function("DATE_NOW",                    "AQL_DATE_NOW", "", false, false, false, true, true) },
  { "DATE_TIMESTAMP",              Function("DATE_TIMESTAMP",              "AQL_DATE_TIMESTAMP", "ns|ns,ns,ns,ns,ns,ns", true, true, false, true, true) },
  { "DATE_ISO8601",                Function("DATE_ISO8601",                "AQL_DATE_ISO8601", "ns|ns,ns,ns,ns,ns,ns", true, true, false, true, true) },
  { "DATE_DAYOFWEEK",              Function("DATE_DAYOFWEEK",              "AQL_DATE_DAYOFWEEK", "ns", true, true, false, true, true) },
  { "DATE_YEAR",                   Function("DATE_YEAR",                   "AQL_DATE_YEAR", "ns", true, true, false, true, true) },
  { "DATE_MONTH",                  Function("DATE_MONTH",                  "AQL_DATE_MONTH", "ns", true, true, false, true, true) },
  { "DATE_DAY",                    Function("DATE_DAY",                    "AQL_DATE_DAY", "ns", true, true, false, true, true) },
  { "DATE_HOUR",                   Function("DATE_HOUR",                   "AQL_DATE_HOUR", "ns", true, true, false, true, true) },
  { "DATE_MINUTE",                 Function("DATE_MINUTE",                 "AQL_DATE_MINUTE", "ns", true, true, false, true, true) },
  { "DATE_SECOND",                 Function("DATE_SECOND",                 "AQL_DATE_SECOND", "ns", true, true, false, true, true) },
  { "DATE_MILLISECOND",            Function("DATE_MILLISECOND",            "AQL_DATE_MILLISECOND", "ns", true, true, false, true, true) },
  { "DATE_DAYOFYEAR",              Function("DATE_DAYOFYEAR",              "AQL_DATE_DAYOFYEAR", "ns", true, true, false, true, true) },
  { "DATE_ISOWEEK",                Function("DATE_ISOWEEK",                "AQL_DATE_ISOWEEK", "ns", true, true, false, true, true) },
  { "DATE_LEAPYEAR",               Function("DATE_LEAPYEAR",               "AQL_DATE_LEAPYEAR", "ns", true, true, false, true, true) },
  { "DATE_QUARTER",                Function("DATE_QUARTER",                "AQL_DATE_QUARTER", "ns", true, true, false, true, true) },
  { "DATE_DAYS_IN_MONTH",          Function("DATE_DAYS_IN_MONTH",          "AQL_DATE_DAYS_IN_MONTH", "ns", true, true, false, true, true) },
  { "DATE_ADD",                    Function("DATE_ADD",                    "AQL_DATE_ADD", "ns,ns|n", true, true, false, true, true) },
  { "DATE_SUBTRACT",               Function("DATE_SUBTRACT",               "AQL_DATE_SUBTRACT", "ns,ns|n", true, true, false, true, true) },
  { "DATE_DIFF",                   Function("DATE_DIFF",                   "AQL_DATE_DIFF", "ns,ns,s|b", true, true, false, true, true) },
  { "DATE_COMPARE",                Function("DATE_COMPARE",                "AQL_DATE_COMPARE", "ns,ns,s|s", true, true, false, true, true) },
  { "DATE_FORMAT",                 Function("DATE_FORMAT",                 "AQL_DATE_FORMAT", "ns,s", true, true, false, true, true) },

  // misc functions
  { "FAIL",                        Function("FAIL",                        "AQL_FAIL", "|s", false, false, true, true, true) },
  { "PASSTHRU",                    Function("PASSTHRU",                    "AQL_PASSTHRU", ".", false, false, false, true, true, &Functions::Passthru ) },
  { "NOOPT",                       Function("NOOPT",                       "AQL_PASSTHRU", ".", false, false, false, true, true, &Functions::Passthru ) },
  { "V8",                          Function("V8",                          "AQL_PASSTHRU", ".", false, false, false, true, true) },
#ifdef TRI_ENABLE_FAILURE_TESTS
  { "TEST_MODIFY",                 Function("TEST_MODIFY",                 "AQL_TEST_MODIFY", "s,.", false, false, false, true, false) },
#endif
  { "SLEEP",                       Function("SLEEP",                       "AQL_SLEEP", "n", false, false, true, true, true) },
  { "COLLECTIONS",                 Function("COLLECTIONS",                 "AQL_COLLECTIONS", "", false, false, true, false, true) },
  { "NOT_NULL",                    Function("NOT_NULL",                    "AQL_NOT_NULL", ".|+", true, true, false, true, true) },
  { "FIRST_LIST",                  Function("FIRST_LIST",                  "AQL_FIRST_LIST", ".|+", true, true, false, true, true) },
  { "FIRST_DOCUMENT",              Function("FIRST_DOCUMENT",              "AQL_FIRST_DOCUMENT", ".|+", true, true, false, true, true) },
  { "PARSE_IDENTIFIER",            Function("PARSE_IDENTIFIER",            "AQL_PARSE_IDENTIFIER", ".", true, true, false, true, true) },
  { "CURRENT_USER",                Function("CURRENT_USER",                "AQL_CURRENT_USER", "", false, false, false, false, true) },
  { "CURRENT_DATABASE",            Function("CURRENT_DATABASE",            "AQL_CURRENT_DATABASE", "", false, false, false, false, true) }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum number of array members / object attributes for considering
/// an array / object literal "big" and pulling it out of the expression
////////////////////////////////////////////////////////////////////////////////

size_t const Executor::DefaultLiteralSizeThreshold = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief maxmium number of array members created from range accesses
////////////////////////////////////////////////////////////////////////////////

int64_t const Executor::MaxRangeAccessArraySize = 1024 * 1024 * 32;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an executor
////////////////////////////////////////////////////////////////////////////////

Executor::Executor (int64_t literalSizeThreshold) 
  : _buffer(nullptr),
    _constantRegisters(),
    _literalSizeThreshold(literalSizeThreshold >= 0 ? static_cast<size_t>(literalSizeThreshold) : DefaultLiteralSizeThreshold) {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an executor
////////////////////////////////////////////////////////////////////////////////

Executor::~Executor () {
  delete _buffer;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an expression execution object
////////////////////////////////////////////////////////////////////////////////

V8Expression* Executor::generateExpression (AstNode const* node) {
  ISOLATE;
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  _constantRegisters.clear();
  detectConstantValues(node, node->type);

  generateCodeExpression(node);

  // std::cout << "Executor::generateExpression: " << std::string(_buffer->c_str(), _buffer->length()) << "\n";
  v8::Handle<v8::Object> constantValues = v8::Object::New(isolate);
  for (auto const& it : _constantRegisters) {
    std::string name = "r";
    name.append(std::to_string(it.second));

    constantValues->ForceSet(TRI_V8_STD_STRING(name), toV8(isolate, it.first));
  }

  // compile the expression
  v8::Handle<v8::Value> func(compileExpression());
  
  // exit early if an error occurred
  HandleV8Error(tryCatch, func);

  // a "simple" expression here is any expression that will only return non-cyclic
  // data and will not return any special JavaScript types such as Date, RegExp or
  // Function
  // as we know that all built-in AQL functions are simple but do not know anything
  // about user-defined functions, we use the following approximation:
  // - all user-defined functions are marked as potentially throwing
  // - some of the internal functions are marked as potentially throwing
  // - if an expression is marked as potentially throwing, it may contain a user-
  //   defined function (which may be non-simple) and is thus considered non-simple
  // - if an expression is marked as not throwing, it will not contain a user-
  //   defined function but only internal AQL functions (which are simple) so the
  //   whole expression is considered simple
  bool isSimple = (! node->canThrow());

  return new V8Expression(isolate, v8::Handle<v8::Function>::Cast(func), constantValues, isSimple);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an expression directly
/// this method is called during AST optimization and will be used to calculate
/// values for constant expressions
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Executor::executeExpression (Query* query,
                                         AstNode const* node) {
  ISOLATE;

  _constantRegisters.clear();
  generateCodeExpression(node);

  // std::cout << "Executor::ExecuteExpression: " << std::string(_buffer->c_str(), _buffer->length()) << "\n";
  v8::TryCatch tryCatch;
  v8::HandleScope scope(isolate);

  // compile the expression
  v8::Handle<v8::Value> func(compileExpression());

  // exit early if an error occurred
  HandleV8Error(tryCatch, func);

  TRI_ASSERT(query != nullptr);

  TRI_GET_GLOBALS();
  v8::Handle<v8::Value> result;
  auto old = v8g->_query;

  try {
    v8g->_query = static_cast<void*>(query);
    TRI_ASSERT(v8g->_query != nullptr);
 
    // execute the function
    v8::Handle<v8::Value> args;
    result = v8::Handle<v8::Function>::Cast(func)->Call(v8::Object::New(isolate), 0, &args);
  
    v8g->_query = old;
  
    // exit if execution raised an error
    HandleV8Error(tryCatch, result);
  }
  catch (...) {
    v8g->_query = old;
    throw;
  }
 

  if (result->IsUndefined()) {
    // undefined => null
    return TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE);
  }
  
  return TRI_ObjectToJson(isolate, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a reference to a built-in function
////////////////////////////////////////////////////////////////////////////////

Function const* Executor::getFunctionByName (std::string const& name) {
  auto it = FunctionNames.find(name);

  if (it == FunctionNames.end()) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN, name.c_str());
  }

  // return the address of the function
  return &((*it).second);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse the expression and note all (big) array/object literals
////////////////////////////////////////////////////////////////////////////////

void Executor::detectConstantValues (AstNode const* node,
                                     AstNodeType previous) {
  if (node == nullptr) {
    return;
  }

  size_t const n = node->numMembers();
 
  if (previous != NODE_TYPE_FCALL &&
      previous != NODE_TYPE_FCALL_USER) {
    // FCALL has an ARRAY node as its immediate child
    // however, we do not want to constify this whole array, but its
    // individual memb 
    // just its individual members
    // otherwise, only the ARRAY node will be marked as constant but not
    // its members. When the code is generated for the function call,
    // the ARRAY node will be ignored because only its individual members
    // (not being marked as const), will be emitted regularly, which would
    // disable the const optimizations if all function call arguments are
    // constants
    if ((node->type == NODE_TYPE_ARRAY ||
         node->type == NODE_TYPE_OBJECT) &&
        n >= _literalSizeThreshold &&
        node->isConstant()) {
      _constantRegisters.emplace(node, _constantRegisters.size());
      return;
    }
  }

  auto nextType = node->type;
  if (previous == NODE_TYPE_FCALL_USER) {
    // FCALL_USER is sticky, so its arguments will not be constified
    nextType = NODE_TYPE_FCALL_USER;
  }
  else if (nextType == NODE_TYPE_FCALL) {
    auto func = static_cast<Function*>(node->getData());

    if (! func->canPassArgumentsByReference) {
      // function should not retrieve its arguments by reference,
      // so we pretend here that it is a user-defined function
      // (user-defined functions will not get their arguments by
      // reference)
      nextType = NODE_TYPE_FCALL_USER;
    }
  }

  for (size_t i = 0; i < n; ++i) {
    detectConstantValues(node->getMemberUnchecked(i), nextType);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert an AST node to a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> Executor::toV8 (v8::Isolate* isolate,
                                      AstNode const* node) const {
  if (node->type == NODE_TYPE_ARRAY) {
    size_t const n = node->numMembers();
    
    v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));
    for (size_t i = 0; i < n; ++i) {
      result->Set(static_cast<uint32_t>(i), toV8(isolate, node->getMember(i)));
    }
    return result;
  }

  if (node->type == NODE_TYPE_OBJECT) {
    size_t const n = node->numMembers();
    
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    for (size_t i = 0; i < n; ++i) {
      auto sub = node->getMember(i);
      result->ForceSet(TRI_V8_PAIR_STRING(sub->getStringValue(), sub->getStringLength()), toV8(isolate, sub->getMember(0)));
    }
    return result;
  }

  if (node->type == NODE_TYPE_VALUE) {
    switch (node->value.type) {
      case VALUE_TYPE_NULL:
        return v8::Null(isolate);
      case VALUE_TYPE_BOOL:
        return v8::Boolean::New(isolate, node->value.value._bool);
      case VALUE_TYPE_INT:
        return v8::Number::New(isolate, static_cast<double>(node->value.value._int));
      case VALUE_TYPE_DOUBLE:
        return v8::Number::New(isolate, static_cast<double>(node->value.value._double));
      case VALUE_TYPE_STRING:
        return TRI_V8_PAIR_STRING(node->value.value._string, node->value.length);
    }
  }
  return v8::Null(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a V8 exception has occurred and throws an appropriate C++ 
/// exception from it if so
////////////////////////////////////////////////////////////////////////////////

void Executor::HandleV8Error (v8::TryCatch& tryCatch,
                              v8::Handle<v8::Value>& result) {
  ISOLATE;

  if (tryCatch.HasCaught()) {
    // caught a V8 exception
    if (! tryCatch.CanContinue()) {
      // request was canceled
      TRI_GET_GLOBALS();
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    // request was not canceled, but some other error occurred
    // peek into the exception
    if (tryCatch.Exception()->IsObject()) {
      // cast the exception to an object

      v8::Handle<v8::Array> objValue = v8::Handle<v8::Array>::Cast(tryCatch.Exception());
      v8::Handle<v8::String> errorNum = TRI_V8_ASCII_STRING("errorNum");
      v8::Handle<v8::String> errorMessage = TRI_V8_ASCII_STRING("errorMessage");
        
      if (objValue->HasOwnProperty(errorNum) && 
          objValue->HasOwnProperty(errorMessage)) {
        v8::Handle<v8::Value> errorNumValue = objValue->Get(errorNum);
        v8::Handle<v8::Value> errorMessageValue = objValue->Get(errorMessage);

        // found something that looks like an ArangoError
        if ((errorNumValue->IsNumber() || errorNumValue->IsNumberObject()) && 
            (errorMessageValue->IsString() || errorMessageValue->IsStringObject())) {
          int errorCode = static_cast<int>(TRI_ObjectToInt64(errorNumValue));
          std::string const errorMessage(TRI_ObjectToString(errorMessageValue));
    
          THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, errorMessage);
        }
      }
    
      // exception is no ArangoError
      std::string const details(TRI_ObjectToString(tryCatch.Exception()));
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_SCRIPT, details);
    }
    
    // we can't figure out what kind of error occurred and throw a generic error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unknown error in scripting");
  }
  
  if (result.IsEmpty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unknown error in scripting");
  }

  // if we get here, no exception has been raised
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compile a V8 function from the code contained in the buffer
////////////////////////////////////////////////////////////////////////////////
  
v8::Handle<v8::Value> Executor::compileExpression () {
  TRI_ASSERT(_buffer != nullptr);
  ISOLATE;

  v8::Handle<v8::Script> compiled = v8::Script::Compile(TRI_V8_STD_STRING((*_buffer)),
                                                        TRI_V8_ASCII_STRING("--script--"));
  
  if (compiled.IsEmpty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to compile v8 expression");
  }
  
  return compiled->Run();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an arbitrary expression
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeExpression (AstNode const* node) {  
  // initialize and/or clear the buffer
  initializeBuffer();
  TRI_ASSERT(_buffer != nullptr);

  // write prologue
  // this checks if global variable _AQL is set and populates if it not
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("(function (vars, consts, reset) { if (_AQL === undefined) { _AQL = require(\"org/arangodb/aql\"); } if (reset) { _AQL.clearCaches(); } return "));

  generateCodeNode(node);

  // write epilogue
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR(";})"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates code for a string value
////////////////////////////////////////////////////////////////////////////////
        
void Executor::generateCodeString (char const* value, 
                                   size_t length) {
  TRI_ASSERT(value != nullptr);

  _buffer->appendChar('"');
  _buffer->appendJsonEncoded(value, length);
  _buffer->appendChar('"');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates code for a string value
////////////////////////////////////////////////////////////////////////////////
        
void Executor::generateCodeString (std::string const& value) {
  _buffer->appendChar('"');
  _buffer->appendJsonEncoded(value.c_str(), value.size());
  _buffer->appendChar('"');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an array
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeArray (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  size_t const n = node->numMembers();

  if (n >= _literalSizeThreshold &&
      node->isConstant()) {

    auto it = _constantRegisters.find(node);

    if (it != _constantRegisters.end()) {
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR("consts.r"));
      _buffer->appendInteger((*it).second);
      return;
    }
  }

  // very conservative minimum bound
  _buffer->reserve(2 + n * 3); 

  _buffer->appendChar('[');
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendChar(',');
    }

    generateCodeNode(node->getMemberUnchecked(i));
  }
  _buffer->appendChar(']');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an array
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeForcedArray (AstNode const* node,
                                        int64_t levels) {
  TRI_ASSERT(node != nullptr);

  if (levels > 1) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.AQL_FLATTEN("));
  }

  bool castToArray = true;
  if (node->type == NODE_TYPE_ARRAY) {
    // value is an array already
    castToArray = false;
  }
  else if (node->type == NODE_TYPE_EXPANSION && 
           node->getMember(0)->type == NODE_TYPE_ARRAY) {
    // value is an expansion over an array
    castToArray = false;
  }
  else if (node->type == NODE_TYPE_ITERATOR && 
           node->getMember(1)->type == NODE_TYPE_ARRAY) {
    castToArray = false;
  }

  if (castToArray) {
    // force the value to be an array
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.AQL_TO_ARRAY("));
    generateCodeNode(node);
    _buffer->appendChar(')');
  } 
  else {
    // value already is an array 
    generateCodeNode(node);
  }

  if (levels > 1) {
    _buffer->appendChar(',');
    _buffer->appendInteger(levels - 1);
    _buffer->appendChar(')');
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an object
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeObject (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  if (node->containsDynamicAttributeName()) {
    generateCodeDynamicObject(node);
  }
  else {
    generateCodeRegularObject(node);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an object with dynamically named
/// attributes
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeDynamicObject (AstNode const* node) {
  size_t const n = node->numMembers();
  // very conservative minimum bound
  _buffer->reserve(64 + n * 10);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("(function() { var o={};"));
  for (size_t i = 0; i < n; ++i) {
    auto member = node->getMemberUnchecked(i);

    if (member->type == NODE_TYPE_OBJECT_ELEMENT) {
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR("o["));
      generateCodeString(member->getStringValue(), member->getStringLength());
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR("]="));
      generateCodeNode(member->getMember(0));
    }
    else {
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR("o[_AQL.AQL_TO_STRING("));
      generateCodeNode(member->getMember(0));
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR(")]="));
      generateCodeNode(member->getMember(1));
    }
    _buffer->appendChar(';');
  }
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("return o;})()"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an object without dynamically named
/// attributes
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeRegularObject (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  size_t const n = node->numMembers();
  
  if (n >= _literalSizeThreshold &&
      node->isConstant()) {

    auto it = _constantRegisters.find(node);

    if (it != _constantRegisters.end()) {
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR("consts.r"));
      _buffer->appendInteger((*it).second);
      return;
    }
  }

  // very conservative minimum bound
  _buffer->reserve(2 + n * 7);

  _buffer->appendChar('{');
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendChar(',');
    }
    
    auto member = node->getMember(i);

    if (member != nullptr) {
      generateCodeString(member->getStringValue(), member->getStringLength());
      _buffer->appendChar(':');
      generateCodeNode(member->getMember(0));
    }
  }
  _buffer->appendChar('}');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a unary operator
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeUnaryOperator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  auto it = InternalFunctionNames.find(static_cast<int>(node->type));

  if (it == InternalFunctionNames.end()) {
    // no function found for the type of node
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "function not found");
  }

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText((*it).second);
  _buffer->appendChar('(');

  generateCodeNode(node->getMember(0));
  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a binary operator
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeBinaryOperator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  auto it = InternalFunctionNames.find(static_cast<int>(node->type));

  if (it == InternalFunctionNames.end()) {
    // no function found for the type of node
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "function not found");
  }
  
  bool wrap = (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
               node->type == NODE_TYPE_OPERATOR_BINARY_OR);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText((*it).second);
  _buffer->appendChar('(');

  if (wrap) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("function () { return "));
    generateCodeNode(node->getMember(0));
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("}, function () { return "));
    generateCodeNode(node->getMember(1));
   _buffer->appendChar('}');
  }
  else {
    generateCodeNode(node->getMember(0));
    _buffer->appendChar(',');
    generateCodeNode(node->getMember(1));
  }

  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for the ternary operator
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeTernaryOperator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 3);

  auto it = InternalFunctionNames.find(static_cast<int>(node->type));

  if (it == InternalFunctionNames.end()) {
    // no function found for the type of node
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "function not found");
  }

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText((*it).second);
  _buffer->appendChar('(');

  generateCodeNode(node->getMember(0));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR(", function () { return "));
  generateCodeNode(node->getMember(1));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("}, function () { return "));
  generateCodeNode(node->getMember(2));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("})"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a variable (read) access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeReference (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
 
  auto variable = static_cast<Variable*>(node->getData());

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("vars["));
  generateCodeString(variable->name);
  _buffer->appendChar(']');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a variable
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeVariable (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
  
  auto variable = static_cast<Variable*>(node->getData());
  
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("vars["));
  generateCodeString(variable->name);
  _buffer->appendChar(']');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a full collection access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeCollection (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
  
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.GET_DOCUMENTS("));
  generateCodeString(node->getStringValue(), node->getStringLength());
  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a call to a built-in function
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeFunctionCall (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);
  
  auto func = static_cast<Function*>(node->getData());

  auto args = node->getMember(0);
  TRI_ASSERT(args != nullptr);
  TRI_ASSERT(args->type == NODE_TYPE_ARRAY);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText(func->internalName);
  _buffer->appendChar('(');

  size_t const n = args->numMembers();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendChar(',');
    }

    auto member = args->getMember(i);

    if (member == nullptr) {
      continue;
    }

    auto conversion = func->getArgumentConversion(i);

    if (member->type == NODE_TYPE_COLLECTION &&
        (conversion == Function::CONVERSION_REQUIRED || conversion == Function::CONVERSION_OPTIONAL)) {
      // the parameter at this position is a collection name that is converted to a string
      // do a parameter conversion from a collection parameter to a collection name parameter
      generateCodeString(member->getStringValue(), member->getStringLength());
    }
    else if (conversion == Function::CONVERSION_REQUIRED) {
      // the parameter at the position is not a collection name... fail
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func->externalName.c_str());
    }
    else {
      generateCodeNode(args->getMember(i));
    }
  }

  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a call to a user-defined function
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeUserFunctionCall (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);
  
  auto args = node->getMember(0);
  TRI_ASSERT(args != nullptr);
  TRI_ASSERT(args->type == NODE_TYPE_ARRAY);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.FCALL_USER("));
  generateCodeString(node->getStringValue(), node->getStringLength());
  _buffer->appendChar(',');

  generateCodeNode(args);
  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an expansion (i.e. [*] operator)
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeExpansion (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  TRI_ASSERT(node->numMembers() == 5);

  auto levels = node->getIntValue(true);

  auto iterator = node->getMember(0);
  auto variable = static_cast<Variable*>(iterator->getMember(0)->getData());

  // start LIMIT 
  auto limitNode = node->getMember(3);

  if (limitNode->type != NODE_TYPE_NOP) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.AQL_SLICE("));
  }

  generateCodeForcedArray(node->getMember(0), levels);

  // FILTER
  auto filterNode = node->getMember(2);

  if (filterNode->type != NODE_TYPE_NOP) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR(".filter(function (v) { "));
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("vars[\""));
    _buffer->appendText(variable->name);
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("\"]=v; "));
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("return _AQL.AQL_TO_BOOL("));
    generateCodeNode(filterNode);
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("); })"));
  }

  // finish LIMIT
  if (limitNode->type != NODE_TYPE_NOP) {
    _buffer->appendChar(',');
    generateCodeNode(limitNode->getMember(0));
    _buffer->appendChar(',');
    generateCodeNode(limitNode->getMember(1));
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR(",true)"));
  }

  // RETURN
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR(".map(function (v) { "));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("vars[\""));
  _buffer->appendText(variable->name);
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("\"]=v; "));

  size_t projectionNode = 1;
  if (node->getMember(4)->type != NODE_TYPE_NOP) {
    projectionNode = 4;
  }

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("return "));
  generateCodeNode(node->getMember(projectionNode));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("; })"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an expansion iterator
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeExpansionIterator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  // intentionally do not stringify node 0
  generateCodeNode(node->getMember(1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a range (i.e. 1..10)
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeRange (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);
  
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.AQL_RANGE("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeNode(node->getMember(1));
  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a named attribute access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeNamedAccess (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.DOCUMENT_MEMBER("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeString(node->getStringValue(), node->getStringLength());
  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a bound attribute access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeBoundAccess (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.DOCUMENT_MEMBER("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeNode(node->getMember(1));
  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an indexed attribute access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeIndexedAccess (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  // indexed access
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.GET_INDEX("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeNode(node->getMember(1));
  _buffer->appendChar(')');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a node
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeNode (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  switch (node->type) {
    case NODE_TYPE_VALUE:
      node->appendValue(_buffer);
      break;

    case NODE_TYPE_ARRAY:
      generateCodeArray(node);
      break;

    case NODE_TYPE_OBJECT:
      generateCodeObject(node);
      break;
    
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
    case NODE_TYPE_OPERATOR_UNARY_NOT:
      generateCodeUnaryOperator(node);
      break;
    
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_BINARY_NIN:
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
    case NODE_TYPE_OPERATOR_BINARY_AND:
    case NODE_TYPE_OPERATOR_BINARY_OR:
      generateCodeBinaryOperator(node);
      break;

    case NODE_TYPE_OPERATOR_TERNARY:
      generateCodeTernaryOperator(node);
      break;
    
    case NODE_TYPE_REFERENCE:
      generateCodeReference(node);
      break;

    case NODE_TYPE_COLLECTION:
      generateCodeCollection(node);
      break;

    case NODE_TYPE_FCALL:
      generateCodeFunctionCall(node);
      break;
    
    case NODE_TYPE_FCALL_USER:
      generateCodeUserFunctionCall(node);
      break;

    case NODE_TYPE_EXPANSION:
      generateCodeExpansion(node);
      break;

    case NODE_TYPE_ITERATOR:
      generateCodeExpansionIterator(node);
      break;
    
    case NODE_TYPE_RANGE:
      generateCodeRange(node);
      break;

    case NODE_TYPE_ATTRIBUTE_ACCESS:
      generateCodeNamedAccess(node);
      break;

    case NODE_TYPE_BOUND_ATTRIBUTE_ACCESS:
      generateCodeBoundAccess(node);
      break;

    case NODE_TYPE_INDEXED_ACCESS:
      generateCodeIndexedAccess(node);
      break;

    case NODE_TYPE_VARIABLE:
    case NODE_TYPE_PARAMETER:
    case NODE_TYPE_PASSTHRU:
    case NODE_TYPE_ARRAY_LIMIT: {
      // we're not expecting these types here
      std::string message("unexpected node type in generateCodeNode: ");
      message.append(node->getTypeString());
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, message);
    }

    default: {
      std::string message("node type not implemented in generateCodeNode: ");
      message.append(node->getTypeString());
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, message);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the string buffer
////////////////////////////////////////////////////////////////////////////////

triagens::basics::StringBuffer* Executor::initializeBuffer () {
  if (_buffer == nullptr) {
    _buffer = new triagens::basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE);

    if (_buffer == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    _buffer->reserve(512);
  }
  else {
    _buffer->clear();
  }

  TRI_ASSERT(_buffer != nullptr);

  return _buffer;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
