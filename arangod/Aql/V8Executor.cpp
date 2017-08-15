////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "V8Executor.h"
#include "Aql/AstNode.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Functions.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/StringBuffer.h"
#include "Basics/Exceptions.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief minimum number of array members / object attributes for considering
/// an array / object literal "big" and pulling it out of the expression
size_t const V8Executor::DefaultLiteralSizeThreshold = 32;

/// @brief creates an executor
V8Executor::V8Executor(int64_t literalSizeThreshold)
    : _buffer(nullptr),
      _constantRegisters(),
      _literalSizeThreshold(literalSizeThreshold >= 0
                                ? static_cast<size_t>(literalSizeThreshold)
                                : DefaultLiteralSizeThreshold) {}

/// @brief destroys an executor
V8Executor::~V8Executor() { delete _buffer; }

/// @brief generates an expression execution object
V8Expression* V8Executor::generateExpression(AstNode const* node) {
  ISOLATE;
  v8::HandleScope scope(isolate);

  v8::TryCatch tryCatch;
  _constantRegisters.clear();
  detectConstantValues(node, node->type);

  _userFunctions.clear();
  detectUserFunctions(node);

  generateCodeExpression(node);

  // std::cout << "V8Executor::generateExpression: " <<
  // std::string(_buffer->c_str(), _buffer->length()) << "\n";
  v8::Handle<v8::Object> constantValues = v8::Object::New(isolate);
  for (auto const& it : _constantRegisters) {
    std::string name = "r";
    name.append(std::to_string(it.second));

    constantValues->ForceSet(TRI_V8_STD_STRING(name), toV8(isolate, it.first));
  }

  TRI_ASSERT(_buffer != nullptr);

  v8::Handle<v8::Script> compiled = v8::Script::Compile(
      TRI_V8_STD_STRING((*_buffer)), TRI_V8_ASCII_STRING("--script--"));

  if (!compiled.IsEmpty()) {
    v8::Handle<v8::Value> func(compiled->Run());

    // exit early if an error occurred
    HandleV8Error(tryCatch, func,  _buffer, false);

    // a "simple" expression here is any expression that will only return
    // non-cyclic
    // data and will not return any special JavaScript types such as Date, RegExp
    // or
    // Function
    // as we know that all built-in AQL functions are simple but do not know
    // anything
    // about user-defined functions, so we expect them to be non-simple
    bool const isSimple = (!node->callsUserDefinedFunction());

    return new V8Expression(isolate, v8::Handle<v8::Function>::Cast(func),
                            constantValues, isSimple);
  }
  else {
    v8::Handle<v8::Value> empty;
    HandleV8Error(tryCatch, empty,  _buffer, true);
    
    // well we're almost sure we never reach this since the above call should throw:
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to compile AQL script code");
  }
}

/// @brief executes an expression directly
/// this method is called during AST optimization and will be used to calculate
/// values for constant expressions
int V8Executor::executeExpression(Query* query, AstNode const* node, 
                                VPackBuilder& builder) {
  ISOLATE;

  _constantRegisters.clear();
  generateCodeExpression(node);

  // std::cout << "V8Executor::ExecuteExpression: " <<
  // std::string(_buffer->c_str(), _buffer->length()) << "\n";
  v8::HandleScope scope(isolate);
  v8::TryCatch tryCatch;


  TRI_ASSERT(_buffer != nullptr);

  v8::Handle<v8::Script> compiled = v8::Script::Compile(
      TRI_V8_STD_STRING((*_buffer)), TRI_V8_ASCII_STRING("--script--"));

  if (!compiled.IsEmpty()) {

    v8::Handle<v8::Value> func(compiled->Run());

    // exit early if an error occurred
    HandleV8Error(tryCatch, func,  _buffer, false);

    TRI_ASSERT(query != nullptr);

    TRI_GET_GLOBALS();
    v8::Handle<v8::Value> result;
    auto old = v8g->_query;

    try {
      v8g->_query = static_cast<void*>(query);
      TRI_ASSERT(v8g->_query != nullptr);

      // execute the function
      v8::Handle<v8::Value> args[] = { v8::Object::New(isolate), v8::Object::New(isolate) };
      result = v8::Handle<v8::Function>::Cast(func)
        ->Call(v8::Object::New(isolate), 2, args);

      v8g->_query = old;

      // exit if execution raised an error
      HandleV8Error(tryCatch, result,  _buffer, false);
    } catch (...) {
      v8g->_query = old;
      throw;
    }
    if (result->IsUndefined()) {
      // undefined => null
      builder.add(VPackValue(VPackValueType::Null));
      return TRI_ERROR_NO_ERROR;
    }
    return TRI_V8ToVPack(isolate, builder, result, false);
  }
  else {
    v8::Handle<v8::Value> empty;
    HandleV8Error(tryCatch, empty, _buffer, true);

    // well we're almost sure we never reach this since the above call should throw:
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unable to compile AQL script code");
  }
}

/// @brief returns a reference to a built-in function
Function const* V8Executor::getFunctionByName(std::string const& name) {
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  return functions->byName(name);
}

/// @brief traverse the expression and note all user-defined functions
void V8Executor::detectUserFunctions(AstNode const* node) {
  if (node == nullptr) {
    return;
  }

  if (node->type == NODE_TYPE_FCALL_USER) {
    _userFunctions.emplace(node->getString(), _userFunctions.size());
  }

  size_t const n = node->numMembers();
  for (size_t i = 0; i < n; ++i) {
    detectUserFunctions(node->getMemberUnchecked(i));
  }
}

/// @brief traverse the expression and note all (big) array/object literals
void V8Executor::detectConstantValues(AstNode const* node, AstNodeType previous) {
  if (node == nullptr) {
    return;
  }

  size_t const n = node->numMembers();

  if (previous != NODE_TYPE_FCALL && previous != NODE_TYPE_FCALL_USER) {
    // FCALL has an ARRAY node as its immediate child
    // however, we do not want to constify this whole array, but just its 
    // individual members
    // otherwise, only the ARRAY node will be marked as constant but not
    // its members. When the code is generated for the function call,
    // the ARRAY node will be ignored because only its individual members
    // (not being marked as const), will be emitted regularly, which would
    // disable the const optimizations if all function call arguments are
    // constants
    if ((node->type == NODE_TYPE_ARRAY || node->type == NODE_TYPE_OBJECT) &&
        n >= _literalSizeThreshold && node->isConstant()) {
      _constantRegisters.emplace(node, _constantRegisters.size());
      return;
    }
  }

  auto nextType = node->type;
  if (previous == NODE_TYPE_FCALL_USER) {
    // FCALL_USER is sticky, so its arguments will not be constified
    nextType = NODE_TYPE_FCALL_USER;
  } else if (nextType == NODE_TYPE_FCALL) {
    auto func = static_cast<Function*>(node->getData());

    if (!func->canPassArgumentsByReference) {
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

/// @brief convert an AST node to a V8 object
v8::Handle<v8::Value> V8Executor::toV8(v8::Isolate* isolate,
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
      result->ForceSet(
          TRI_V8_PAIR_STRING(sub->getStringValue(), sub->getStringLength()),
          toV8(isolate, sub->getMember(0)));
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
        return v8::Number::New(isolate,
                               static_cast<double>(node->value.value._int));
      case VALUE_TYPE_DOUBLE:
        return v8::Number::New(isolate,
                               static_cast<double>(node->value.value._double));
      case VALUE_TYPE_STRING:
        return TRI_V8_PAIR_STRING(node->value.value._string,
                                  node->value.length);
    }
  }
  return v8::Null(isolate);
}

/// @brief checks if a V8 exception has occurred and throws an appropriate C++
/// exception from it if so
void V8Executor::HandleV8Error(v8::TryCatch& tryCatch,
                             v8::Handle<v8::Value>& result,
                             arangodb::basics::StringBuffer* const buffer,
                             bool duringCompile) {
  ISOLATE;

  if (tryCatch.HasCaught()) {
    // caught a V8 exception
    if (!tryCatch.CanContinue()) {
      // request was canceled
      TRI_GET_GLOBALS();
      v8g->_canceled = true;

      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    // request was not canceled, but some other error occurred
    // peek into the exception
    if (tryCatch.Exception()->IsObject()) {
      // cast the exception to an object

      v8::Handle<v8::Array> objValue =
          v8::Handle<v8::Array>::Cast(tryCatch.Exception());
      v8::Handle<v8::String> errorNum = TRI_V8_ASCII_STRING("errorNum");
      v8::Handle<v8::String> errorMessage = TRI_V8_ASCII_STRING("errorMessage");

      TRI_Utf8ValueNFC stacktrace(TRI_UNKNOWN_MEM_ZONE, tryCatch.StackTrace());

      if (objValue->HasOwnProperty(errorNum) &&
          objValue->HasOwnProperty(errorMessage)) {
        v8::Handle<v8::Value> errorNumValue = objValue->Get(errorNum);
        v8::Handle<v8::Value> errorMessageValue = objValue->Get(errorMessage);

        // found something that looks like an ArangoError
        if ((errorNumValue->IsNumber() || errorNumValue->IsNumberObject()) &&
            (errorMessageValue->IsString() ||
             errorMessageValue->IsStringObject())) {
          int errorCode = static_cast<int>(TRI_ObjectToInt64(errorNumValue));
          std::string errorMessage(TRI_ObjectToString(errorMessageValue));

          if (*stacktrace && stacktrace.length() > 0) {
            errorMessage += "\nstacktrace of offending AQL function: ";
            errorMessage += *stacktrace;
          }

          THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, errorMessage);
        }
      }

      // exception is no ArangoError
      std::string details(TRI_ObjectToString(tryCatch.Exception()));

      if (buffer) {
        std::string script(buffer->c_str(), buffer->length());
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << details << " " << script;
        details += "\nSee log for more details";
      }
      if (*stacktrace && stacktrace.length() > 0) {
        details += "\nstacktrace of offending AQL function: ";
        details += *stacktrace;
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_SCRIPT, details);
    }

    std::string msg("unknown error in scripting");
    if (duringCompile) {
      msg += " (during compilation)";
    }
    if (buffer) {
      std::string script(buffer->c_str(), buffer->length());
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << msg << " " << script;
      msg += " See log for details";
    }
    // we can't figure out what kind of error occurred and throw a generic error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }

  if (result.IsEmpty()) {
    std::string msg("unknown error in scripting");
    if (duringCompile) {
      msg += " (during compilation)";
    }
    if (buffer) {
      std::string script(buffer->c_str(), buffer->length());
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << msg << " " << script;
      msg += " See log for details";
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }

  // if we get here, no exception has been raised
}

/// @brief generate JavaScript code for an arbitrary expression
void V8Executor::generateCodeExpression(AstNode const* node) {
  // initialize and/or clear the buffer
  initializeBuffer();
  TRI_ASSERT(_buffer != nullptr);

  // write prologue
  // this checks if global variable _AQL is set and populates if it not
  // the check is only performed if "state.i" (=init) is not yet set
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR(
      "(function (vars, state, consts) { "
      "if (!state.i) { "
      "if (_AQL === undefined) { "
      "_AQL = require(\"@arangodb/aql\"); } "
      "_AQL.clearCaches(); "));

  // lookup all user-defined functions used and store them in variables
  // "state.f\d+"
  for (auto const& it : _userFunctions) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("state.f"));
    _buffer->appendInteger(it.second);
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR(" = _AQL.lookupFunction(\""));
    _buffer->appendText(it.first);
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("\", {}); "));
  }
 
  // generate specialized functions for UDFs 
  for (auto const& it : _userFunctions) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("state.e"));
    _buffer->appendInteger(it.second);
    // "state.e\d+" executes the user function in a wrapper, converting the 
    // function result back into the allowed range, and catching any errors
    // thrown by the function 
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR(" = function(params) { try { return _AQL.fixValue(state.f"));
    _buffer->appendInteger(it.second);
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR(".apply({ name: \""));
    _buffer->appendText(it.first);
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("\" }, params)); } catch (err) { _AQL.throwFromFunction(\""));
    _buffer->appendText(it.first);
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("\", require(\"internal\").errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR, _AQL.AQL_TO_STRING(err.stack || String(err))); } }; "));
  }
   
  // set "state.i" to true (=initialized) 
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("state.i = true; } return "));

  generateCodeNode(node);

  // write epilogue
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("; })"));
}

/// @brief generates code for a string value
void V8Executor::generateCodeString(char const* value, size_t length) {
  TRI_ASSERT(value != nullptr);

  _buffer->appendJsonEncoded(value, length);
}

/// @brief generates code for a string value
void V8Executor::generateCodeString(std::string const& value) {
  _buffer->appendJsonEncoded(value.c_str(), value.size());
}

/// @brief generate JavaScript code for an array
void V8Executor::generateCodeArray(AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  size_t const n = node->numMembers();

  if (n >= _literalSizeThreshold && node->isConstant()) {
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

/// @brief generate JavaScript code for an array
void V8Executor::generateCodeForcedArray(AstNode const* node, int64_t levels) {
  TRI_ASSERT(node != nullptr);

  if (levels > 1) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.AQL_FLATTEN("));
  }

  bool castToArray = true;
  if (node->type == NODE_TYPE_ARRAY) {
    // value is an array already
    castToArray = false;
  } else if (node->type == NODE_TYPE_EXPANSION &&
             node->getMember(0)->type == NODE_TYPE_ARRAY) {
    // value is an expansion over an array
    castToArray = false;
  } else if (node->type == NODE_TYPE_ITERATOR &&
             node->getMember(1)->type == NODE_TYPE_ARRAY) {
    castToArray = false;
  }

  if (castToArray) {
    // force the value to be an array
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.AQL_TO_ARRAY("));
    generateCodeNode(node);
    _buffer->appendText(", false");
    _buffer->appendChar(')');
  } else {
    // value already is an array
    generateCodeNode(node);
  }

  if (levels > 1) {
    _buffer->appendChar(',');
    _buffer->appendInteger(levels - 1);
    _buffer->appendChar(')');
  }
}

/// @brief generate JavaScript code for an object
void V8Executor::generateCodeObject(AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  if (node->containsDynamicAttributeName()) {
    generateCodeDynamicObject(node);
  } else {
    generateCodeRegularObject(node);
  }
}

/// @brief generate JavaScript code for an object with dynamically named
/// attributes
void V8Executor::generateCodeDynamicObject(AstNode const* node) {
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
    } else {
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR("o[_AQL.AQL_TO_STRING("));
      generateCodeNode(member->getMember(0));
      _buffer->appendText(TRI_CHAR_LENGTH_PAIR(")]="));
      generateCodeNode(member->getMember(1));
    }
    _buffer->appendChar(';');
  }
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("return o;})()"));
}

/// @brief generate JavaScript code for an object without dynamically named
/// attributes
void V8Executor::generateCodeRegularObject(AstNode const* node) {
  TRI_ASSERT(node != nullptr);

  size_t const n = node->numMembers();

  if (n >= _literalSizeThreshold && node->isConstant()) {
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

/// @brief generate JavaScript code for a unary operator
void V8Executor::generateCodeUnaryOperator(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  TRI_ASSERT(functions != nullptr);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText(functions->getOperatorName(node->type, "unary operator function not found"));
  _buffer->appendChar('(');

  generateCodeNode(node->getMember(0));
  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for a binary operator
void V8Executor::generateCodeBinaryOperator(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  TRI_ASSERT(functions != nullptr);

  bool wrap = (node->type == NODE_TYPE_OPERATOR_BINARY_AND ||
               node->type == NODE_TYPE_OPERATOR_BINARY_OR);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText(functions->getOperatorName(node->type, "binary operator function not found"));
  _buffer->appendChar('(');

  if (wrap) {
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("function () { return "));
    generateCodeNode(node->getMember(0));
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("}, function () { return "));
    generateCodeNode(node->getMember(1));
    _buffer->appendChar('}');
  } else {
    generateCodeNode(node->getMember(0));
    _buffer->appendChar(',');
    generateCodeNode(node->getMember(1));
  }

  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for a binary array operator
void V8Executor::generateCodeBinaryArrayOperator(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 3);
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  TRI_ASSERT(functions != nullptr);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText(functions->getOperatorName(node->type, "binary array function not found"));
  _buffer->appendChar('(');

  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeNode(node->getMember(1));

  AstNode const* quantifier = node->getMember(2);

  if (quantifier->type == NODE_TYPE_QUANTIFIER) {
    _buffer->appendChar(',');
    _buffer->appendInteger(quantifier->getIntValue(true));
  }

  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for the ternary operator
void V8Executor::generateCodeTernaryOperator(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 3);
  auto functions = AqlFunctionFeature::AQLFUNCTIONS;
  TRI_ASSERT(functions != nullptr);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
  _buffer->appendText(functions->getOperatorName(node->type, "function not found"));
  _buffer->appendChar('(');

  generateCodeNode(node->getMember(0));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR(", function () { return "));
  generateCodeNode(node->getMember(1));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("}, function () { return "));
  generateCodeNode(node->getMember(2));
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("})"));
}

/// @brief generate JavaScript code for a variable (read) access
void V8Executor::generateCodeReference(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);

  auto variable = static_cast<Variable*>(node->getData());

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("vars["));
  generateCodeString(variable->name);
  _buffer->appendChar(']');
}

/// @brief generate JavaScript code for a variable
void V8Executor::generateCodeVariable(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);

  auto variable = static_cast<Variable*>(node->getData());

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("vars["));
  generateCodeString(variable->name);
  _buffer->appendChar(']');
}

/// @brief generate JavaScript code for a full collection access
void V8Executor::generateCodeCollection(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 0);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.GET_DOCUMENTS("));
  generateCodeString(node->getStringValue(), node->getStringLength());
  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for a call to a built-in function
void V8Executor::generateCodeFunctionCall(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  auto func = static_cast<Function*>(node->getData());

  auto args = node->getMember(0);
  TRI_ASSERT(args != nullptr);
  TRI_ASSERT(args->type == NODE_TYPE_ARRAY);

  if (func->externalName != "V8") {
    // special case for the V8 function... this is actually not a function 
    // call at all, but a wrapper to ensure that the following expression
    // is executed using V8
    _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL."));
    _buffer->appendText(func->internalName);
  }
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
        (conversion == Function::CONVERSION_REQUIRED ||
         conversion == Function::CONVERSION_OPTIONAL)) {
      // the parameter at this position is a collection name that is converted
      // to a string
      // do a parameter conversion from a collection parameter to a collection
      // name parameter
      generateCodeString(member->getStringValue(), member->getStringLength());
    } else if (conversion == Function::CONVERSION_REQUIRED) {
      // the parameter at the position is not a collection name... fail
      THROW_ARANGO_EXCEPTION_PARAMS(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
          func->externalName.c_str());
    } else {
      generateCodeNode(args->getMember(i));
    }
  }

  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for a call to a user-defined function
void V8Executor::generateCodeUserFunctionCall(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  auto args = node->getMember(0);
  TRI_ASSERT(args != nullptr);
  TRI_ASSERT(args->type == NODE_TYPE_ARRAY);

  auto it = _userFunctions.find(node->getString());

  if (it == _userFunctions.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "user function not found");
  }

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("state.e"));
  _buffer->appendInteger((*it).second);
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("("));

  generateCodeNode(args);
  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for an expansion (i.e. [*] operator)
void V8Executor::generateCodeExpansion(AstNode const* node) {
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

/// @brief generate JavaScript code for an expansion iterator
void V8Executor::generateCodeExpansionIterator(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  // intentionally do not stringify node 0
  generateCodeNode(node->getMember(1));
}

/// @brief generate JavaScript code for a range (i.e. 1..10)
void V8Executor::generateCodeRange(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.AQL_RANGE("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeNode(node->getMember(1));
  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for a named attribute access
void V8Executor::generateCodeNamedAccess(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 1);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.DOCUMENT_MEMBER("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeString(node->getStringValue(), node->getStringLength());
  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for a bound attribute access
void V8Executor::generateCodeBoundAccess(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.DOCUMENT_MEMBER("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeNode(node->getMember(1));
  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for an indexed attribute access
void V8Executor::generateCodeIndexedAccess(AstNode const* node) {
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->numMembers() == 2);

  // indexed access
  _buffer->appendText(TRI_CHAR_LENGTH_PAIR("_AQL.GET_INDEX("));
  generateCodeNode(node->getMember(0));
  _buffer->appendChar(',');
  generateCodeNode(node->getMember(1));
  _buffer->appendChar(')');
}

/// @brief generate JavaScript code for a node
void V8Executor::generateCodeNode(AstNode const* node) {
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

    case NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_LE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GT:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_GE:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_IN:
    case NODE_TYPE_OPERATOR_BINARY_ARRAY_NIN:
      generateCodeBinaryArrayOperator(node);
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

/// @brief create the string buffer
arangodb::basics::StringBuffer* V8Executor::initializeBuffer() {
  if (_buffer == nullptr) {
    _buffer = new arangodb::basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE, 512, false);

    if (_buffer->stringBuffer()->_buffer == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  } else {
    _buffer->clear();
  }

  TRI_ASSERT(_buffer != nullptr);

  return _buffer;
}
