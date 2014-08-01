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
// --SECTION--                                  function names used in execution
// -----------------------------------------------------------------------------

std::unordered_map<int, std::string> const V8Executor::FunctionNames{ 
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

  std::cout << "CREATED CODE: " << _buffer->c_str() << "\n";
  
  v8::Handle<v8::Script> compiled = v8::Script::Compile(v8::String::New(_buffer->c_str(), (int) _buffer->length()),
                                                        v8::String::New("--script--"));
  
  if (compiled.IsEmpty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  
  v8::TryCatch tryCatch;
  v8::Handle<v8::Value> val = compiled->Run();

  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    else {
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
  }

  if (val.IsEmpty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
 
  v8::Isolate* isolate = v8::Isolate::GetCurrent(); 
  return new V8Expression(isolate, v8::Persistent<v8::Function>::New(isolate, v8::Handle<v8::Function>::Cast(val)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an expression directly
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* V8Executor::executeExpression (AstNode const* node) {
  generateCodeExpression(node);

  return execute();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for an arbitrary expression
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeExpression (AstNode const* node) {  
  // initialise and/or clear the buffer
  initBuffer();
  TRI_ASSERT(_buffer != nullptr);

  // write prologue
  _buffer->appendText("(function (vars) { require('internal').print(vars); var aql = require(\"org/arangodb/ahuacatl\"); return ");

  generateCodeNode(node);

  // write epilogue
  _buffer->appendText("; })");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a unary operator
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeUnaryOperator (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  auto it = FunctionNames.find(static_cast<int>(node->type));

  if (it == FunctionNames.end()) {
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

  auto it = FunctionNames.find(static_cast<int>(node->type));

  if (it == FunctionNames.end()) {
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

  auto it = FunctionNames.find(static_cast<int>(node->type));

  if (it == FunctionNames.end()) {
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

  _buffer->appendText("vars[\"");
  _buffer->appendJsonEncoded(variable->name.c_str());
  _buffer->appendText("\"]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a variable
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeVariable (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
  
  auto variable = static_cast<Variable*>(node->getData());

  _buffer->appendText("vars[\"");
  _buffer->appendJsonEncoded(variable->name.c_str());
  _buffer->appendText("\"]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a full collection access
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeCollection (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);
  
  char const* name = node->getStringValue();

  _buffer->appendText("aql.GET_DOCUMENTS(\"");
  _buffer->appendJsonEncoded(name);
  _buffer->appendText("\")");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate JavaScript code for a function call
////////////////////////////////////////////////////////////////////////////////

void V8Executor::generateCodeFunctionCall (AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);
  
  char const* name = node->getStringValue();

  _buffer->appendText("aql.");
  _buffer->appendText(name);
  _buffer->appendText("(");

  auto args = node->getMember(0);
  TRI_ASSERT(args != nullptr);
  TRI_ASSERT(args->type == NODE_TYPE_LIST);

  size_t const n = args->numMembers();
  for (size_t i = 0; i < n; ++i) {
    if (i > 0) {
      _buffer->appendText(", ");
    }

    generateCodeNode(args->getMember(i));
  }
  _buffer->appendText(")");
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
  _buffer->appendText(", \"");
  _buffer->appendJsonEncoded(node->getStringValue());
  _buffer->appendText("\")");
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
    case NODE_TYPE_LIST:
    case NODE_TYPE_ARRAY:
      node->append(_buffer);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the contents of the string buffer
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* V8Executor::execute () {
  // note: if this function is called without an already opened handle scope,
  // it will fail badly

  TRI_ASSERT(_buffer != nullptr);
  v8::Isolate* isolate = v8::Isolate::GetCurrent(); 

  v8::Handle<v8::Script> compiled = v8::Script::Compile(v8::String::New(_buffer->c_str(), (int) _buffer->length()),
                                                        v8::String::New("--script--"));
  
  if (compiled.IsEmpty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  
  v8::TryCatch tryCatch;
  v8::Handle<v8::Value> func = compiled->Run();
  
  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    else {
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData());
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
  }

  if (func.IsEmpty() || ! func->IsFunction()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // execute the function
  v8::Handle<v8::Value> args[] = { };
  v8::Handle<v8::Value> result = v8::Handle<v8::Function>::Cast(func)->Call(v8::Object::New(), 0, args);
  
  if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    else {
      TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData());
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }
  }

  if (result.IsEmpty()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  return TRI_ObjectToJson(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
