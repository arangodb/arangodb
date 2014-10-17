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
#include "Utils/Exception.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             static initialization
// -----------------------------------------------------------------------------

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
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_AND),   "LOGICAL_AND_FN" },
  { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_OR),    "LOGICAL_OR_FN" },
  { static_cast<int>(NODE_TYPE_OPERATOR_TERNARY),      "TERNARY_OPERATOR_FN" }
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
  // l = list
  // a = (hash) array/document
  // r = regex (a string with a special format). note: the regex type is mutually exclusive with all other types

  // type check functions
  { "IS_NULL",                     Function("IS_NULL",                     "IS_NULL", ".", true, false, &Functions::IsNull) },
  { "IS_BOOL",                     Function("IS_BOOL",                     "IS_BOOL", ".", true, false, &Functions::IsBool) },
  { "IS_NUMBER",                   Function("IS_NUMBER",                   "IS_NUMBER", ".", true, false, &Functions::IsNumber) },
  { "IS_STRING",                   Function("IS_STRING",                   "IS_STRING", ".", true, false, &Functions::IsString) },
  { "IS_LIST",                     Function("IS_LIST",                     "IS_LIST", ".", true, false, &Functions::IsList) },
  { "IS_DOCUMENT",                 Function("IS_DOCUMENT",                 "IS_DOCUMENT", ".", true, false, &Functions::IsDocument) },
  
  // type cast functions
  { "TO_NUMBER",                   Function("TO_NUMBER",                   "CAST_NUMBER", ".", true, false) },
  { "TO_STRING",                   Function("TO_STRING",                   "CAST_STRING", ".", true, false) },
  { "TO_BOOL",                     Function("TO_BOOL",                     "CAST_BOOL", ".", true, false) },
  { "TO_LIST",                     Function("TO_LIST",                     "CAST_LIST", ".", true, false) },
  
  // string functions
  { "CONCAT",                      Function("CONCAT",                      "STRING_CONCAT", "sz,sz|+", true, true) },
  { "CONCAT_SEPARATOR",            Function("CONCAT_SEPARATOR",            "STRING_CONCAT_SEPARATOR", "s,sz,sz|+", true, true) },
  { "CHAR_LENGTH",                 Function("CHAR_LENGTH",                 "CHAR_LENGTH", "s", true, true) },
  { "LOWER",                       Function("LOWER",                       "STRING_LOWER", "s", true, true) },
  { "UPPER",                       Function("UPPER",                       "STRING_UPPER", "s", true, true) },
  { "SUBSTRING",                   Function("SUBSTRING",                   "STRING_SUBSTRING", "s,n|n", true, true) },
  { "CONTAINS",                    Function("CONTAINS",                    "STRING_CONTAINS", "s,s|b", true, true) },
  { "LIKE",                        Function("LIKE",                        "STRING_LIKE", "s,r|b", true, true) },
  { "LEFT",                        Function("LEFT",                        "STRING_LEFT", "s,n", true, true) },
  { "RIGHT",                       Function("RIGHT",                       "STRING_RIGHT", "s,n", true, true) },
  { "TRIM",                        Function("TRIM",                        "STRING_TRIM", "s|n", true, true) },
  { "FIND_FIRST",                  Function("FIND_FIRST",                  "STRING_FIND_FIRST", "s,s|zn,zn", true, true) },
  { "FIND_LAST",                   Function("FIND_LAST",                   "STRING_FIND_LAST", "s,s|zn,zn", true, true) },

  // numeric functions
  { "FLOOR",                       Function("FLOOR",                       "NUMBER_FLOOR", "n", true, true) },
  { "CEIL",                        Function("CEIL",                        "NUMBER_CEIL", "n", true, true) },
  { "ROUND",                       Function("ROUND",                       "NUMBER_ROUND", "n", true, true) },
  { "ABS",                         Function("ABS",                         "NUMBER_ABS", "n", true, true) },
  { "RAND",                        Function("RAND",                        "NUMBER_RAND", "", false, false) },
  { "SQRT",                        Function("SQRT",                        "NUMBER_SQRT", "n", true, true) },
  
  // list functions
  { "RANGE",                       Function("RANGE",                       "RANGE", "n,n|n", true, true) },
  { "UNION",                       Function("UNION",                       "UNION", "l,l|+",true, true) },
  { "UNION_DISTINCT",              Function("UNION_DISTINCT",              "UNION_DISTINCT", "l,l|+", true, true) },
  { "MINUS",                       Function("MINUS",                       "MINUS", "l,l|+", true, true) },
  { "INTERSECTION",                Function("INTERSECTION",                "INTERSECTION", "l,l|+", true, true) },
  { "FLATTEN",                     Function("FLATTEN",                     "FLATTEN", "l|n", true, true) },
  { "LENGTH",                      Function("LENGTH",                      "LENGTH", "las", true, true) },
  { "MIN",                         Function("MIN",                         "MIN", "l", true, true) },
  { "MAX",                         Function("MAX",                         "MAX", "l", true, true) },
  { "SUM",                         Function("SUM",                         "SUM", "l", true, true) },
  { "MEDIAN",                      Function("MEDIAN",                      "MEDIAN", "l", true, true) }, 
  { "AVERAGE",                     Function("AVERAGE",                     "AVERAGE", "l", true, true) },
  { "VARIANCE_SAMPLE",             Function("VARIANCE_SAMPLE",             "VARIANCE_SAMPLE", "l", true, true) },
  { "VARIANCE_POPULATION",         Function("VARIANCE_POPULATION",         "VARIANCE_POPULATION", "l", true, true) },
  { "STDDEV_SAMPLE",               Function("STDDEV_SAMPLE",               "STDDEV_SAMPLE", "l", true, true) },
  { "STDDEV_POPULATION",           Function("STDDEV_POPULATION",           "STDDEV_POPULATION", "l", true, true) },
  { "UNIQUE",                      Function("UNIQUE",                      "UNIQUE", "l", true, true) },
  { "SLICE",                       Function("SLICE",                       "SLICE", "l,n|n", true, true) },
  { "REVERSE",                     Function("REVERSE",                     "REVERSE", "ls", true, true) },    // note: REVERSE() can be applied on strings, too
  { "FIRST",                       Function("FIRST",                       "FIRST", "l", true, true) },
  { "LAST",                        Function("LAST",                        "LAST", "l", true, true) },
  { "NTH",                         Function("NTH",                         "NTH", "l,n", true, true) },
  { "POSITION",                    Function("POSITION",                    "POSITION", "l,.|b", true, true) },

  // document functions
  { "HAS",                         Function("HAS",                         "HAS", "az,s", true, true) },
  { "ATTRIBUTES",                  Function("ATTRIBUTES",                  "ATTRIBUTES", "a|b,b", true, true) },
  { "MERGE",                       Function("MERGE",                       "MERGE", "a,a|+", true, true) },
  { "MERGE_RECURSIVE",             Function("MERGE_RECURSIVE",             "MERGE_RECURSIVE", "a,a|+", true, true) },
  { "DOCUMENT",                    Function("DOCUMENT",                    "DOCUMENT", "h.|.", false, true) },
  { "MATCHES",                     Function("MATCHES",                     "MATCHES", ".,l|b", true, true) },
  { "UNSET",                       Function("UNSET",                       "UNSET", "a,sl|+", true, true) },
  { "KEEP",                        Function("KEEP",                        "KEEP", "a,sl|+", true, true) },
  { "TRANSLATE",                   Function("TRANSLATE",                   "TRANSLATE", ".,a|.", true, true) },

  // geo functions
  { "NEAR",                        Function("NEAR",                        "GEO_NEAR", "h,n,n|nz,s", false, true) },
  { "WITHIN",                      Function("WITHIN",                      "GEO_WITHIN", "h,n,n,n|s", false, true) },

  // fulltext functions
  { "FULLTEXT",                    Function("FULLTEXT",                    "FULLTEXT", "h,s,s", false, true) },

  // graph functions
  { "PATHS",                       Function("PATHS",                       "GRAPH_PATHS", "c,h|s,b", false, true) },
  { "GRAPH_PATHS",                 Function("GRAPH_PATHS",                 "GENERAL_GRAPH_PATHS", "s|a", false, true) },
  { "SHORTEST_PATH",               Function("SHORTEST_PATH",               "GRAPH_SHORTEST_PATH", "h,h,s,s,s|a", false, true) },
  { "GRAPH_SHORTEST_PATH",         Function("GRAPH_SHORTEST_PATH",         "GENERAL_GRAPH_SHORTEST_PATH", "s,als,als|a", false, true) },
  { "GRAPH_DISTANCE_TO",           Function("GRAPH_DISTANCE_TO",           "GENERAL_GRAPH_DISTANCE_TO", "s,als,als|a", false, true) },
  { "TRAVERSAL",                   Function("TRAVERSAL",                   "GRAPH_TRAVERSAL", "h,h,s,s|a", false, true) },
  { "GRAPH_TRAVERSAL",             Function("GRAPH_TRAVERSAL",             "GENERAL_GRAPH_TRAVERSAL", "s,als,s|a", false, true) },
  { "TRAVERSAL_TREE",              Function("TRAVERSAL_TREE",              "GRAPH_TRAVERSAL_TREE", "h,h,s,s,s|a", false, true) },
  { "GRAPH_TRAVERSAL_TREE",        Function("GRAPH_TRAVERSAL_TREE",        "GENERAL_GRAPH_TRAVERSAL_TREE", "s,als,s,s|a", false, true) },
  { "EDGES",                       Function("EDGES",                       "GRAPH_EDGES", "h,s,s|l", false, true) },
  { "GRAPH_EDGES",                 Function("GRAPH_EDGES",                 "GENERAL_GRAPH_EDGES", "s,als|a", false, true) },
  { "GRAPH_VERTICES",              Function("GRAPH_VERTICES",              "GENERAL_GRAPH_VERTICES", "s,als|a", false, true) },
  { "NEIGHBORS",                   Function("NEIGHBORS",                   "GRAPH_NEIGHBORS", "h,h,s,s|l", false, true) },
  { "GRAPH_NEIGHBORS",             Function("GRAPH_NEIGHBORS",             "GENERAL_GRAPH_NEIGHBORS", "s,als|a", false, true) },
  { "GRAPH_COMMON_NEIGHBORS",      Function("GRAPH_COMMON_NEIGHBORS",      "GENERAL_GRAPH_COMMON_NEIGHBORS", "s,als,als|a,a", false, true) },
  { "GRAPH_COMMON_PROPERTIES",     Function("GRAPH_COMMON_PROPERTIES",     "GENERAL_GRAPH_COMMON_PROPERTIES", "s,als,als|a", false, true) },
  { "GRAPH_ECCENTRICITY",          Function("GRAPH_ECCENTRICITY",          "GENERAL_GRAPH_ECCENTRICITY", "s|a", false, true) },
  { "GRAPH_BETWEENNESS",           Function("GRAPH_BETWEENNESS",           "GENERAL_GRAPH_BETWEENNESS", "s|a", false, true) },
  { "GRAPH_CLOSENESS",             Function("GRAPH_CLOSENESS",             "GENERAL_GRAPH_CLOSENESS", "s|a", false, true) },
  { "GRAPH_ABSOLUTE_ECCENTRICITY", Function("GRAPH_ABSOLUTE_ECCENTRICITY", "GENERAL_GRAPH_ABSOLUTE_ECCENTRICITY", "s,als|a", false, true) },
  { "GRAPH_ABSOLUTE_BETWEENNESS",  Function("GRAPH_ABSOLUTE_BETWEENNESS",  "GENERAL_GRAPH_ABSOLUTE_BETWEENNESS", "s,als|a", false, true) },
  { "GRAPH_ABSOLUTE_CLOSENESS",    Function("GRAPH_ABSOLUTE_CLOSENESS",    "GENERAL_GRAPH_ABSOLUTE_CLOSENESS", "s,als|a", false, true) },
  { "GRAPH_DIAMETER",              Function("GRAPH_DIAMETER",              "GENERAL_GRAPH_DIAMETER", "s|a", false, true) },
  { "GRAPH_RADIUS",                Function("GRAPH_RADIUS",                "GENERAL_GRAPH_RADIUS", "s|a", false, true) },

  // date functions
  { "DATE_NOW",                    Function("DATE_NOW",                    "DATE_NOW", "", false, false) },
  { "DATE_TIMESTAMP",              Function("DATE_TIMESTAMP",              "DATE_TIMESTAMP", "ns|ns,ns,ns,ns,ns,ns", true, true) },
  { "DATE_ISO8601",                Function("DATE_ISO8601",                "DATE_ISO8601", "ns|ns,ns,ns,ns,ns,ns", true, true) },
  { "DATE_DAYOFWEEK",              Function("DATE_DAYOFWEEK",              "DATE_DAYOFWEEK", "ns", true, true) },
  { "DATE_YEAR",                   Function("DATE_YEAR",                   "DATE_YEAR", "ns", true, true) },
  { "DATE_MONTH",                  Function("DATE_MONTH",                  "DATE_MONTH", "ns", true, true) },
  { "DATE_DAY",                    Function("DATE_DAY",                    "DATE_DAY", "ns", true, true) },
  { "DATE_HOUR",                   Function("DATE_HOUR",                   "DATE_HOUR", "ns", true, true) },
  { "DATE_MINUTE",                 Function("DATE_MINUTE",                 "DATE_MINUTE", "ns", true, true) },
  { "DATE_SECOND",                 Function("DATE_SECOND",                 "DATE_SECOND", "ns", true, true) },
  { "DATE_MILLISECOND",            Function("DATE_MILLISECOND",            "DATE_MILLISECOND", "ns", true, true) },

  // misc functions
  { "FAIL",                        Function("FAIL",                        "FAIL", "|s", false, true) },
  { "PASSTHRU",                    Function("PASSTHRU",                    "PASSTHRU", ".", false, true) },
  { "SLEEP",                       Function("SLEEP",                       "SLEEP", "n", false, false) },
  { "COLLECTIONS",                 Function("COLLECTIONS",                 "COLLECTIONS", "", false, true) },
  { "NOT_NULL",                    Function("NOT_NULL",                    "NOT_NULL", ".|+", true, true) },
  { "FIRST_LIST",                  Function("FIRST_LIST",                  "FIRST_LIST", ".|+", true, false) },
  { "FIRST_DOCUMENT",              Function("FIRST_DOCUMENT",              "FIRST_DOCUMENT", ".|+", true, false) },
  { "PARSE_IDENTIFIER",            Function("PARSE_IDENTIFIER",            "PARSE_IDENTIFIER", ".", true, true) },
  { "SKIPLIST",                    Function("SKIPLIST",                    "SKIPLIST_QUERY", "h,a|n,n", false, true) },
  { "CURRENT_USER",                Function("CURRENT_USER",                "CURRENT_USER", "", false, false) },
  { "CURRENT_DATABASE",            Function("CURRENT_DATABASE",            "CURRENT_DATABASE", "", false, false) }
};

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an executor
////////////////////////////////////////////////////////////////////////////////

Executor::Executor () :
  _buffer(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an executor
////////////////////////////////////////////////////////////////////////////////

Executor::~Executor () {
  if (_buffer != nullptr) {
    delete _buffer;
    _buffer = nullptr;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an expression execution object
////////////////////////////////////////////////////////////////////////////////

V8Expression* Executor::generateExpression (AstNode const* node) {
  generateCodeExpression(node);
  
  // std::cout << "Executor::generateExpression: " << _buffer->c_str() << "\n";

  v8::TryCatch tryCatch;
  // compile the expression
  v8::Handle<v8::Value> func(compileExpression());
  
  // exit early if an error occurred
  HandleV8Error(tryCatch, func);

  v8::Isolate* isolate = v8::Isolate::GetCurrent(); 
  return new V8Expression(isolate, v8::Persistent<v8::Function>::New(isolate, v8::Handle<v8::Function>::Cast(func)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an expression directly
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Executor::executeExpression (AstNode const* node) {
  generateCodeExpression(node);

  // std::cout << "Executor::ExecuteExpression: " << _buffer->c_str() << "\n";
  
  // note: if this function is called without an already opened handle scope,
  // it will fail badly

  v8::TryCatch tryCatch;
  // compile the expression
  v8::Handle<v8::Value> func(compileExpression());

  // exit early if an error occurred
  HandleV8Error(tryCatch, func);

  // execute the function
  v8::Handle<v8::Value> args;
  v8::Handle<v8::Value> result = v8::Handle<v8::Function>::Cast(func)->Call(v8::Object::New(), 0, &args);
 
  // exit if execution raised an error
  HandleV8Error(tryCatch, result);

  return TRI_ObjectToJson(result);
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
/// @brief checks if a V8 exception has occurred and throws an appropriate C++ 
/// exception from it if so
////////////////////////////////////////////////////////////////////////////////

void Executor::HandleV8Error (v8::TryCatch& tryCatch,
                              v8::Handle<v8::Value>& result) {
  if (tryCatch.HasCaught()) {
    // caught a V8 exception
    if (! tryCatch.CanContinue()) {
      // request was cancelled
      v8::Isolate* isolate = v8::Isolate::GetCurrent(); 
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData());
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    // request was not cancelled, but some other error occurred
    // peek into the exception
    if (tryCatch.Exception()->IsObject()) {
      // cast the exception to an object
      v8::Handle<v8::Array> objValue = v8::Handle<v8::Array>::Cast(tryCatch.Exception());
      v8::Handle<v8::String> errorNum = v8::String::New("errorNum");
      v8::Handle<v8::String> errorMessage = v8::String::New("errorMessage");
        
      if (objValue->HasOwnProperty(errorNum) && 
          objValue->HasOwnProperty(errorMessage)) {
        v8::Handle<v8::Value> errorNumValue = objValue->Get(errorNum);
        v8::Handle<v8::Value> errorMessageValue = objValue->Get(errorMessage);

        // found something that looks like an ArangoError
        if (errorNumValue->IsNumber() && 
            errorMessageValue->IsString()) {
          int errorCode = static_cast<int>(TRI_ObjectToInt64(errorNumValue));
          std::string const errorMessage(TRI_ObjectToString(errorMessageValue));
    
          THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, errorMessage);
        }
      }
    
      // exception is no ArangoError
      std::string const details(TRI_ObjectToString(tryCatch.Exception()));
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_SCRIPT, details);
    }
    
    // we can't figure out what kind of error occured and throw a generic error
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  
  if (result.IsEmpty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // if we get here, no exception has been raised
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compile a V8 function from the code contained in the buffer
////////////////////////////////////////////////////////////////////////////////
  
v8::Handle<v8::Value> Executor::compileExpression () {
  TRI_ASSERT(_buffer != nullptr);

  v8::Handle<v8::Script> compiled = v8::Script::Compile(v8::String::New(_buffer->c_str(), (int) _buffer->length()),
                                                        v8::String::New("--script--"));
  
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
  _buffer->appendText("(function (vars) { var aql = require(\"org/arangodb/aql\"); return ");

  generateCodeNode(node);

  // write epilogue
  _buffer->appendText("; })");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates code for a string value
////////////////////////////////////////////////////////////////////////////////
        
void Executor::generateCodeString (char const* value) {
  TRI_ASSERT(value != nullptr);

  _buffer->appendChar('"');
  _buffer->appendJsonEncoded(value);
  _buffer->appendChar('"');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates code for a string value
////////////////////////////////////////////////////////////////////////////////
        
void Executor::generateCodeString (std::string const& value) {
  _buffer->appendChar('"');
  _buffer->appendJsonEncoded(value.c_str());
  _buffer->appendChar('"');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a list
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeList (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  size_t const n = node->numMembers();

  _buffer->appendText("[ ");
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendText(", ");
    }

    generateCodeNode(node->getMember(i));
  }
  _buffer->appendText(" ]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an array
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeArray (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  size_t const n = node->numMembers();

  _buffer->appendText("{ ");
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendText(", ");
    }
    
    auto member = node->getMember(i);

    if (member != nullptr) {
      generateCodeString(member->getStringValue());
      _buffer->appendText(" : ");
      generateCodeNode(member->getMember(0));
    }
  }
  _buffer->appendText(" }");
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

  _buffer->appendText("aql.");
  _buffer->appendText((*it).second);
  _buffer->appendText("(");

  generateCodeNode(node->getMember(0));
  _buffer->appendText(")");
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

  _buffer->appendText("aql.");
  _buffer->appendText((*it).second);
  _buffer->appendText("(");

  if (wrap) {
    _buffer->appendText("function () { return ");
    generateCodeNode(node->getMember(0));
    _buffer->appendText("}, function () { return ");
    generateCodeNode(node->getMember(1));
   _buffer->appendText("}");
  }
  else {
    generateCodeNode(node->getMember(0));
    _buffer->appendText(", ");
    generateCodeNode(node->getMember(1));
  }

  _buffer->appendText(")");
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

  _buffer->appendText("aql.");
  _buffer->appendText((*it).second);
  _buffer->appendText("(");

  generateCodeNode(node->getMember(0));
  _buffer->appendText(", function () { return ");
  generateCodeNode(node->getMember(1));
  _buffer->appendText("}, function () { return ");
  generateCodeNode(node->getMember(2));
  _buffer->appendText("})");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a variable (read) access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeReference (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
 
  auto variable = static_cast<Variable*>(node->getData());

  _buffer->appendText("vars[");
  generateCodeString(variable->name.c_str());
  _buffer->appendText("]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a variable
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeVariable (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
  
  auto variable = static_cast<Variable*>(node->getData());
  
  _buffer->appendText("vars[");
  generateCodeString(variable->name.c_str());
  _buffer->appendText("]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a full collection access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeCollection (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
  
  char const* name = node->getStringValue();

  _buffer->appendText("aql.GET_DOCUMENTS(");
  generateCodeString(name);
  _buffer->appendText(")");
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
  TRI_ASSERT(args->type == NODE_TYPE_LIST);

  _buffer->appendText("aql.");
  _buffer->appendText(func->internalName);
  _buffer->appendText("(");

  size_t const n = args->numMembers();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendText(", ");
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
      char const* name = member->getStringValue();
      generateCodeString(name);
    }
    else if (conversion == Function::CONVERSION_REQUIRED) {
      // the parameter at the position is not a collection name... fail
      THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func->internalName.c_str());
    }
    else {
      generateCodeNode(args->getMember(i));
    }
  }

  _buffer->appendText(")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a call to a user-defined function
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeUserFunctionCall (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);
  
  char const* name = node->getStringValue();
  TRI_ASSERT(name != nullptr);

  auto args = node->getMember(0);
  TRI_ASSERT(args != nullptr);
  TRI_ASSERT(args->type == NODE_TYPE_LIST);

  _buffer->appendText("aql.FCALL_USER(");
  generateCodeString(name);
  _buffer->appendText(", [");

  size_t const n = args->numMembers();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendText(", ");
    }

    generateCodeNode(args->getMember(i));
  }
  _buffer->appendText("])");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an expansion (i.e. [*] operator)
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeExpand (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  _buffer->appendText("(function () { var r = []; ");
  generateCodeNode(node->getMember(0));
  _buffer->appendText(".forEach(function (v) { ");
  auto iterator = node->getMember(0);
  auto variable = static_cast<Variable*>(iterator->getMember(0)->getData());
  _buffer->appendText("vars[\"");
  _buffer->appendText(variable->name);
  _buffer->appendText("\"] = v; ");

  _buffer->appendText("r.push(");
  generateCodeNode(node->getMember(1));
  _buffer->appendText("); }); return r; })()");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an expansion iterator
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeExpandIterator (AstNode const* node) {
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
  
  _buffer->appendText("aql.RANGE(");
  generateCodeNode(node->getMember(0));
  _buffer->appendText(", ");
  generateCodeNode(node->getMember(1));
  _buffer->appendText(")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a named attribute access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeNamedAccess (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  _buffer->appendText("aql.DOCUMENT_MEMBER(");
  generateCodeNode(node->getMember(0));
  _buffer->appendText(", ");
  generateCodeString(node->getStringValue());
  _buffer->appendText(")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a bound attribute access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeBoundAccess (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  _buffer->appendText("aql.DOCUMENT_MEMBER(");
  generateCodeNode(node->getMember(0));
  _buffer->appendText(", ");
  generateCodeNode(node->getMember(1));
  _buffer->appendText(")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an indexed attribute access
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeIndexedAccess (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  _buffer->appendText("aql.GET_INDEX(");
  generateCodeNode(node->getMember(0));
  _buffer->appendText(", ");
  generateCodeNode(node->getMember(1));
  _buffer->appendText(")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a node
////////////////////////////////////////////////////////////////////////////////

void Executor::generateCodeNode (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  switch (node->type) {
    case NODE_TYPE_VALUE:
      node->append(_buffer, true);
      break;

    case NODE_TYPE_LIST:
      generateCodeList(node);
      break;

    case NODE_TYPE_ARRAY:
      generateCodeArray(node);
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
    
    case NODE_TYPE_EXPAND:
      generateCodeExpand(node);
      break;

    case NODE_TYPE_ITERATOR:
      generateCodeExpandIterator(node);
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
      // we're not expecting these types here
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected node type in code generator");

    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "node type not implemented");
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

    _buffer->reserve(256);
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
