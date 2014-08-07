////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, v8 context executor
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

#include "Aql/V8Executor.h"
#include "Aql/AstNode.h"
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

std::unordered_map<int, std::string const> const V8Executor::InternalFunctionNames{ 
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

std::unordered_map<std::string, Function const> const V8Executor::FunctionNames{ 
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
  { "IS_NULL",                     Function("IS_NULL", ".", true) },
  { "IS_BOOL",                     Function("IS_BOOL", ".", true) },
  { "IS_NUMBER",                   Function("IS_NUMBER", ".", true) },
  { "IS_STRING",                   Function("IS_STRING", ".", true) },
  { "IS_LIST",                     Function("IS_LIST", ".", true) },
  { "IS_DOCUMENT",                 Function("IS_DOCUMENT", ".", true) },
  
  // type cast functions
  { "TO_NUMBER",                   Function("CAST_NUMBER", ".", true) },
  { "TO_STRING",                   Function("CAST_STRING", ".", true) },
  { "TO_BOOL",                     Function("CAST_BOOL", ".", true) },
  { "TO_LIST",                     Function("CAST_LIST", ".", true) },
  
  // string functions
  { "CONCAT",                      Function("STRING_CONCAT", "sz,sz|+", true) },
  { "CONCAT_SEPARATOR",            Function("STRING_CONCAT_SEPARATOR", "s,sz,sz|+", true) },
  { "CHAR_LENGTH",                 Function("CHAR_LENGTH", "s", true) },
  { "LOWER",                       Function("STRING_LOWER", "s", true) },
  { "UPPER",                       Function("STRING_UPPER", "s", true) },
  { "SUBSTRING",                   Function("STRING_SUBSTRING", "s,n|n", true) },
  { "CONTAINS",                    Function("STRING_CONTAINS", "s,s|b", true) },
  { "LIKE",                        Function("STRING_LIKE", "s,r|b", true) },
  { "LEFT",                        Function("STRING_LEFT", "s,n", true) },
  { "RIGHT",                       Function("STRING_RIGHT", "s,n", true) },
  { "TRIM",                        Function("STRING_TRIM", "s|n", true) },

  // numeric functions
  { "FLOOR",                       Function("NUMBER_FLOOR", "n", true) },
  { "CEIL",                        Function("NUMBER_CEIL", "n", true) },
  { "ROUND",                       Function("NUMBER_ROUND", "n", true) },
  { "ABS",                         Function("NUMBER_ABS", "n", true) },
  { "RAND",                        Function("NUMBER_RAND", "", false) },
  { "SQRT",                        Function("NUMBER_SQRT", "n", true) },
  
  // list functions
  { "RANGE",                       Function("RANGE", "n,n|n", true) },
  { "UNION",                       Function("UNION", "l,l|+",true) },
  { "UNION_DISTINCT",              Function("UNION_DISTINCT", "l,l|+", true) },
  { "MINUS",                       Function("MINUS", "l,l|+", true) },
  { "INTERSECTION",                Function("INTERSECTION", "l,l|+", true) },
  { "FLATTEN",                     Function("FLATTEN", "l|n", true) },
  { "LENGTH",                      Function("LENGTH", "las", true) },
  { "MIN",                         Function("MIN", "l", true) },
  { "MAX",                         Function("MAX", "l", true) },
  { "SUM",                         Function("SUM", "l", true) },
  { "MEDIAN",                      Function("MEDIAN", "l", true) }, 
  { "AVERAGE",                     Function("AVERAGE", "l", true) },
  { "VARIANCE_SAMPLE",             Function("VARIANCE_SAMPLE", "l", true) },
  { "VARIANCE_POPULATION",         Function("VARIANCE_POPULATION", "l", true) },
  { "STDDEV_SAMPLE",               Function("STDDEV_SAMPLE", "l", true) },
  { "STDDEV_POPULATION",           Function("STDDEV_POPULATION", "l", true) },
  { "UNIQUE",                      Function("UNIQUE", "l", true) },
  { "SLICE",                       Function("SLICE", "l,n|n", true) },
  { "REVERSE",                     Function("REVERSE", "ls", true) },    // note: REVERSE() can be applied on strings, too
  { "FIRST",                       Function("FIRST", "l", true) },
  { "LAST",                        Function("LAST", "l", true) },
  { "NTH",                         Function("NTH", "l,n", true) },
  { "POSITION",                    Function("POSITION", "l,.|b", true) },

  // document functions
  { "HAS",                         Function("HAS", "az,s", true) },
  { "ATTRIBUTES",                  Function("ATTRIBUTES", "a|b,b", true) },
  { "MERGE",                       Function("MERGE", "a,a|+", true) },
  { "MERGE_RECURSIVE",             Function("MERGE_RECURSIVE", "a,a|+", true) },
  { "DOCUMENT",                    Function("DOCUMENT", "h.|.", true) },
  { "MATCHES",                     Function("MATCHES", ".,l|b", true) },
  { "UNSET",                       Function("UNSET", "a,sl|+", true) },
  { "KEEP",                        Function("KEEP", "a,sl|+", true) },
  { "TRANSLATE",                   Function("TRANSLATE", ".,a|.", true) },

  // geo functions
  { "NEAR",                        Function("GEO_NEAR", "h,n,n|nz,s", false) },
  { "WITHIN",                      Function("GEO_WITHIN", "h,n,n,n|s", false) },

  // fulltext functions
  { "FULLTEXT",                    Function("FULLTEXT", "h,s,s", false) },

  // graph functions
  { "PATHS",                       Function("GRAPH_PATHS", "c,h|s,b", false )},
  { "GRAPH_PATHS",                 Function("GENERAL_GRAPH_PATHS", "s|a", false )},
  { "SHORTEST_PATH",               Function("GRAPH_SHORTEST_PATH", "h,h,s,s,s|a", false )},
  { "GRAPH_SHORTEST_PATH",         Function("GENERAL_GRAPH_SHORTEST_PATH", "s,als,als|a", false )},
  { "GRAPH_DISTANCE_TO",           Function("GENERAL_GRAPH_DISTANCE_TO", "s,als,als|a", false )},
  { "TRAVERSAL",                   Function("GRAPH_TRAVERSAL", "h,h,s,s|a", false )},
  { "GRAPH_TRAVERSAL",             Function("GENERAL_GRAPH_TRAVERSAL", "s,als,s|a", false )},
  { "TRAVERSAL_TREE",              Function("GRAPH_TRAVERSAL_TREE", "s,als,s,s|a", false )},
  { "GRAPH_TRAVERSAL_TREE",        Function("GENERAL_GRAPH_TRAVERSAL_TREE", "s,als,s,s|a", false )},
  { "EDGES",                       Function("GRAPH_EDGES", "h,s,s|l", false )},
  { "GRAPH_EDGES",                 Function("GENERAL_GRAPH_EDGES", "s,als|a", false )},
  { "GRAPH_VERTICES",              Function("GENERAL_GRAPH_VERTICES", "s,als|a", false )},
  { "NEIGHBORS",                   Function("GRAPH_NEIGHBORS", "h,h,s,s|l", false )},
  { "GRAPH_NEIGHBORS",             Function("GENERAL_GRAPH_NEIGHBORS", "s,als|a", false )},
  { "GRAPH_COMMON_NEIGHBORS",      Function("GENERAL_GRAPH_COMMON_NEIGHBORS", "s,als,als|a,a", false )},
  { "GRAPH_COMMON_PROPERTIES",     Function("GENERAL_GRAPH_COMMON_PROPERTIES", "s,als,als|a", false )},
  { "GRAPH_ECCENTRICITY",          Function("GENERAL_GRAPH_ECCENTRICITY", "s|a", false )},
  { "GRAPH_BETWEENNESS",           Function("GENERAL_GRAPH_BETWEENNESS", "s|a", false )},
  { "GRAPH_CLOSENESS",             Function("GENERAL_GRAPH_CLOSENESS", "s|a", false )},
  { "GRAPH_ABSOLUTE_ECCENTRICITY", Function("GENERAL_GRAPH_ABSOLUTE_ECCENTRICITY", "s,als|a", false )},
  { "GRAPH_ABSOLUTE_CLOSENESS",    Function("GENERAL_GRAPH_ABSOLUTE_CLOSENESS", "s,als|a", false )},
  { "GRAPH_DIAMETER",              Function("GENERAL_GRAPH_DIAMETER", "s|a", false )},
  { "GRAPH_RADIUS",                Function("GENERAL_GRAPH_RADIUS", "s|a", false )},

  // date functions
  { "DATE_NOW",                    Function("DATE_NOW", "", false) },
  { "DATE_TIMESTAMP",              Function("DATE_TIMESTAMP", "ns|ns,ns,ns,ns,ns,ns", true) },
  { "DATE_ISO8601",                Function("DATE_ISO8601", "ns|ns,ns,ns,ns,ns,ns", true) },
  { "DATE_DAYOFWEEK",              Function("DATE_DAYOFWEEK", "ns", true) },
  { "DATE_YEAR",                   Function("DATE_YEAR", "ns", true) },
  { "DATE_MONTH",                  Function("DATE_MONTH", "ns", true) },
  { "DATE_DAY",                    Function("DATE_DAY", "ns", true) },
  { "DATE_HOUR",                   Function("DATE_HOUR", "ns", true) },
  { "DATE_MINUTE",                 Function("DATE_MINUTE", "ns", true) },
  { "DATE_SECOND",                 Function("DATE_SECOND", "ns", true) },
  { "DATE_MILLISECOND",            Function("DATE_MILLISECOND", "ns", true) },

  // misc functions
  { "FAIL",                        Function("FAIL", "|s", false) },
  { "PASSTHRU",                    Function("PASSTHRU", ".", false) },
  { "SLEEP",                       Function("SLEEP", "n", false) },
  { "COLLECTIONS",                 Function("COLLECTIONS", "", false) },
  { "NOT_NULL",                    Function("NOT_NULL", ".|+", true) },
  { "FIRST_LIST",                  Function("FIRST_LIST", ".|+", true) },
  { "FIRST_DOCUMENT",              Function("FIRST_DOCUMENT", ".|+", true) },
  { "PARSE_IDENTIFIER",            Function("PARSE_IDENTIFIER", ".", true) },
  { "SKIPLIST",                    Function("SKIPLIST_QUERY", "h,a|n,n", false) },
  { "CURRENT_USER",                Function("CURRENT_USER", "", false) },
  { "CURRENT_DATABASE",            Function("CURRENT_DATABASE", "", false) }
};

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an executor
////////////////////////////////////////////////////////////////////////////////

V8Executor::V8Executor () :
  _buffer(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an executor
////////////////////////////////////////////////////////////////////////////////

V8Executor::~V8Executor () {
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

V8Expression* V8Executor::generateExpression (AstNode const* node) {
  generateCodeExpression(node);
  
  std::cout << "Executor::generateExpression: " << _buffer->c_str() << "\n";

  v8::TryCatch tryCatch;
  // compile the expression
  v8::Handle<v8::Value> func = compileExpression();
  
  // exit early if an error occurred
  HandleV8Error(tryCatch, func);

  v8::Isolate* isolate = v8::Isolate::GetCurrent(); 
  return new V8Expression(isolate, v8::Persistent<v8::Function>::New(isolate, v8::Handle<v8::Function>::Cast(func)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an expression directly
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* V8Executor::executeExpression (AstNode const* node) {
  generateCodeExpression(node);

  std::cout << "Executor::ExecuteExpression: " << _buffer->c_str() << "\n";
  
  // note: if this function is called without an already opened handle scope,
  // it will fail badly

  v8::TryCatch tryCatch;
  // compile the expression
  v8::Handle<v8::Value> func = compileExpression();

  // exit early if an error occurred
  HandleV8Error(tryCatch, func);

  // execute the function
  v8::Handle<v8::Value> args[] = { };
  v8::Handle<v8::Value> result = v8::Handle<v8::Function>::Cast(func)->Call(v8::Object::New(), 0, args);
 
  // exit if execution raised an error
  HandleV8Error(tryCatch, result);

  return TRI_ObjectToJson(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a reference to a built-in function
////////////////////////////////////////////////////////////////////////////////

Function const* V8Executor::getFunctionByName (std::string const& name) {
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

void V8Executor::HandleV8Error (v8::TryCatch& tryCatch,
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
  
v8::Handle<v8::Value> V8Executor::compileExpression () {
  TRI_ASSERT(_buffer != nullptr);

  v8::Handle<v8::Script> compiled = v8::Script::Compile(v8::String::New(_buffer->c_str(), (int) _buffer->length()),
                                                        v8::String::New("--script--"));
  
  if (compiled.IsEmpty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  
  return compiled->Run();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an arbitrary expression
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeExpression (AstNode const* node) {  
  // initialise and/or clear the buffer
  initBuffer();
  TRI_ASSERT(_buffer != nullptr);

  // write prologue
  _buffer->appendText("(function (vars) { var aql = require(\"org/arangodb/ahuacatl\"); return ");

  generateCodeNode(node);

  // write epilogue
  _buffer->appendText("; })");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates code for a string value
////////////////////////////////////////////////////////////////////////////////
        
void V8Executor::generateCodeString (char const* value) {
  TRI_ASSERT(value != nullptr);

  _buffer->appendChar('"');
  _buffer->appendJsonEncoded(value);
  _buffer->appendChar('"');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a list
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeList (AstNode const* node) {
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

void V8Executor::generateCodeArray (AstNode const* node) {
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

void V8Executor::generateCodeUnaryOperator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  auto it = InternalFunctionNames.find(static_cast<int>(node->type));

  if (it == InternalFunctionNames.end()) {
    // no function found for the type of node
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
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

void V8Executor::generateCodeBinaryOperator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  auto it = InternalFunctionNames.find(static_cast<int>(node->type));

  if (it == InternalFunctionNames.end()) {
    // no function found for the type of node
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
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

void V8Executor::generateCodeTernaryOperator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 3);

  auto it = InternalFunctionNames.find(static_cast<int>(node->type));

  if (it == InternalFunctionNames.end()) {
    // no function found for the type of node
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
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

void V8Executor::generateCodeReference (AstNode const* node) {
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

void V8Executor::generateCodeVariable (AstNode const* node) {
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

void V8Executor::generateCodeCollection (AstNode const* node) {
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

void V8Executor::generateCodeFunctionCall (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);
  
  auto func = static_cast<Function*>(node->getData());

  auto args = node->getMember(0);
  TRI_ASSERT(args != nullptr);
  TRI_ASSERT(args->type == NODE_TYPE_LIST);

  _buffer->appendText("aql.");
  _buffer->appendText(func->name);
  _buffer->appendText("(");

  size_t const n = args->numMembers();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendText(", ");
    }

    auto member = args->getMember(i);

    if (member != nullptr) {
      if (member->type == NODE_TYPE_COLLECTION &&
          func->containsCollectionParameter) {
        // do a parameter conversion from a collection parameter to a collection name parameter
        char const* name = member->getStringValue();
        generateCodeString(name);
      }
      else {
        // generate regular code for the node
        generateCodeNode(args->getMember(i));
      }
    }
  }
  _buffer->appendText(")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a call to a user-defined function
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeUserFunctionCall (AstNode const* node) {
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

void V8Executor::generateCodeExpand (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  _buffer->appendText("(function () { var r = []; return ");
  generateCodeNode(node->getMember(0));
  _buffer->appendText(".forEach(function (v) { r.push_back(");
  generateCodeNode(node->getMember(1));
  _buffer->appendText("); }); })()");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an expansion iterator
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeExpandIterator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  // intentionally do not stringify node 0
  generateCodeNode(node->getMember(1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a range (i.e. 1..10)
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeRange (AstNode const* node) {
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

void V8Executor::generateCodeNamedAccess (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  _buffer->appendText("aql.DOCUMENT_MEMBER(");
  generateCodeNode(node->getMember(0));
  _buffer->appendText(", ");
  generateCodeString(node->getStringValue());
  _buffer->appendText(")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an indexed attribute access
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeIndexedAccess (AstNode const* node) {
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

void V8Executor::generateCodeNode (AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  switch (node->type) {
    case NODE_TYPE_VALUE:
      node->append(_buffer);
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
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
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

    case NODE_TYPE_INDEXED_ACCESS:
      generateCodeIndexedAccess(node);
      break;

    case NODE_TYPE_VARIABLE:
      // we're not expecting a variable here
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);

    default:
      // TODO: remove debug output
      std::cout << "NODE TYPE NOT IMPLEMENTED (" << node->typeString() << ")\n";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the string buffer
////////////////////////////////////////////////////////////////////////////////

triagens::basics::StringBuffer* V8Executor::initBuffer () {
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
