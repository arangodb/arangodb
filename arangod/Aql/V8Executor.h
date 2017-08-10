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

#ifndef ARANGOD_AQL_V8EXECUTOR_H
#define ARANGOD_AQL_V8EXECUTOR_H 1

#include "Basics/Common.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "V8/v8-globals.h"

namespace arangodb {
namespace basics {
class StringBuffer;
}

namespace velocypack {
class Builder;
}

namespace aql {
struct AstNode;
struct Function;
class Query;
struct V8Expression;

class V8Executor {
 public:
  /// @brief create the executor
  explicit V8Executor(int64_t);

  /// @brief destroy the executor
  ~V8Executor();

 public:
  /// @brief generates an expression execution object
  V8Expression* generateExpression(AstNode const*);

  /// @brief executes an expression directly
  int executeExpression(Query*, AstNode const*, arangodb::velocypack::Builder&);

  /// @brief returns a reference to a built-in function
  Function const* getFunctionByName(std::string const&);

  /// @brief checks if a V8 exception has occurred and throws an appropriate C++
  /// exception from it if so
  static void HandleV8Error(v8::TryCatch&, v8::Handle<v8::Value>&, arangodb::basics::StringBuffer* const, bool duringCompile);

 private:
  /// @brief traverse the expression and note all user-defined functions
  void detectUserFunctions(AstNode const*);

  /// @brief traverse the expression and note all (big) array/object literals
  void detectConstantValues(AstNode const*, AstNodeType);

  /// @brief convert an AST node to a V8 object
  v8::Handle<v8::Value> toV8(v8::Isolate*, AstNode const*) const;

  /// @brief generate JavaScript code for an arbitrary expression
  void generateCodeExpression(AstNode const*);

  /// @brief generates code for a string value
  void generateCodeString(char const*, size_t);

  /// @brief generates code for a string value
  void generateCodeString(std::string const&);

  /// @brief generate JavaScript code for an array
  void generateCodeArray(AstNode const*);

  /// @brief generate JavaScript code for a forced array
  void generateCodeForcedArray(AstNode const*, int64_t);

  /// @brief generate JavaScript code for an object
  void generateCodeObject(AstNode const*);

  /// @brief generate JavaScript code for an object with dynamically named
  /// attributes
  void generateCodeDynamicObject(AstNode const*);

  /// @brief generate JavaScript code for an object without dynamically named
  /// attributes
  void generateCodeRegularObject(AstNode const*);

  /// @brief generate JavaScript code for a unary operator
  void generateCodeUnaryOperator(AstNode const*);

  /// @brief generate JavaScript code for a binary operator
  void generateCodeBinaryOperator(AstNode const*);

  /// @brief generate JavaScript code for a binary array operator
  void generateCodeBinaryArrayOperator(AstNode const*);

  /// @brief generate JavaScript code for the ternary operator
  void generateCodeTernaryOperator(AstNode const*);

  /// @brief generate JavaScript code for a variable (read) access
  void generateCodeReference(AstNode const*);

  /// @brief generate JavaScript code for a variable
  void generateCodeVariable(AstNode const*);

  /// @brief generate JavaScript code for a full collection access
  void generateCodeCollection(AstNode const*);

  /// @brief generate JavaScript code for a call to a built-in function
  void generateCodeFunctionCall(AstNode const*);

  /// @brief generate JavaScript code for a user-defined function
  void generateCodeUserFunctionCall(AstNode const*);

  /// @brief generate JavaScript code for an expansion (i.e. [*] operator)
  void generateCodeExpansion(AstNode const*);

  /// @brief generate JavaScript code for an expansion iterator
  void generateCodeExpansionIterator(AstNode const*);

  /// @brief generate JavaScript code for a range (i.e. 1..10)
  void generateCodeRange(AstNode const*);

  /// @brief generate JavaScript code for a named attribute access
  void generateCodeNamedAccess(AstNode const*);

  /// @brief generate JavaScript code for a named attribute access
  void generateCodeBoundAccess(AstNode const*);

  /// @brief generate JavaScript code for an indexed attribute access
  void generateCodeIndexedAccess(AstNode const*);

  /// @brief generate JavaScript code for a node
  void generateCodeNode(AstNode const*);

  /// @brief create the string buffer
  arangodb::basics::StringBuffer* initializeBuffer();

 private:
  /// @brief a string buffer used for operations
  arangodb::basics::StringBuffer* _buffer;

  /// @brief mapping from literal array/objects to register ids
  std::unordered_map<AstNode const*, size_t> _constantRegisters;
  
  /// @brief mapping from user-defined function names to register ids
  std::unordered_map<std::string, size_t> _userFunctions;

  /// @brief local value for literal object size threshold
  size_t const _literalSizeThreshold;

 public:
  /// @brief minimum number of array members / object attributes for considering
  /// an array / object literal "big" and pulling it out of the expression
  static size_t const DefaultLiteralSizeThreshold;

};
}
}

#endif
