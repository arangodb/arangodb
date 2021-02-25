////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Parser.h"

#include "Basics/Common.h"
#include "Basics/ScopeGuard.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Aql/QueryResult.h"
#include "Aql/QueryString.h"

#include <sstream>

using namespace arangodb::aql;

/// @brief create the parser
Parser::Parser(QueryContext& query,
               Ast& ast, QueryString& qs)
    : _query(query),
      _ast(ast),
      _queryString(qs),
      _scanner(nullptr),
      _queryStringStart(nullptr),
      _buffer(nullptr),
      _remainingLength(0),
      _offset(0),
      _marker(nullptr),
      _stack() {
  _stack.reserve(4);

  _queryStringStart = _queryString.data();
  _buffer = qs.data();
  _remainingLength = qs.size();
}

/// @brief destroy the parser
Parser::~Parser() = default;

/// @brief set data for write queries
bool Parser::configureWriteQuery(AstNode const* collectionNode, AstNode* optionNode) {
  bool isExclusiveAccess = false;

  if (optionNode != nullptr) {
    // already validated at parse-time
    TRI_ASSERT(optionNode->isObject());
    TRI_ASSERT(optionNode->isConstant());
    isExclusiveAccess = ExecutionPlan::hasExclusiveAccessOption(optionNode);
  }

  // now track which collection is going to be modified
  _ast.addWriteCollection(collectionNode, isExclusiveAccess);

  // register that we have seen a modification operation
  _ast.setContainsModificationNode();

  return true;
}

/// @brief parse the query
void Parser::parse() {
  if (queryString().empty() || remainingLength() == 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_EMPTY);
  }

  // start main scope
  auto scopes = _ast.scopes();
  scopes->start(AQL_SCOPE_MAIN);

  TRI_ASSERT(_scanner == nullptr);
  if (Aqllex_init(&_scanner) != 0) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_ASSERT(_scanner != nullptr);
  Aqlset_extra(this, _scanner);

  auto guard = scopeGuard([this]() {
    Aqllex_destroy(_scanner);
    _scanner = nullptr;
  });

  // parse the query string
  if (Aqlparse(this)) {
    // lexing/parsing failed
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_PARSE);
  }

  // end main scope
  TRI_ASSERT(scopes->numActive() > 0);

  while (scopes->numActive() > 0) {
    scopes->endCurrent();
  }

  TRI_ASSERT(scopes->numActive() == 0);
}

/// @brief parse the query and retun parse details
QueryResult Parser::parseWithDetails() {
  parse();

  QueryResult result;
  result.collectionNames = _query.collections().collectionNames();
  result.bindParameters = _ast.bindParameters();
  auto builder = std::make_shared<VPackBuilder>();
  _ast.toVelocyPack(*builder, false);
  result.data = std::move(builder); 

  return result;
}

/// @brief register a parse error, position is specified as line / column
void Parser::registerParseError(ErrorCode errorCode, char const* format,
                                std::string_view data, int line, int column) {
  char buffer[512];
  // make sure the buffer is always initialized
  buffer[0] = '\0';
  buffer[sizeof(buffer) - 1] = '\0';

  snprintf(buffer, sizeof(buffer) - 1, format, data.data());

  return registerParseError(errorCode, buffer, line, column);
}

/// @brief register a parse error, position is specified as line / column
void Parser::registerParseError(ErrorCode errorCode, std::string_view data, int line, int column) {
  TRI_ASSERT(errorCode != TRI_ERROR_NO_ERROR);
  TRI_ASSERT(data.data() != nullptr);

  // extract the query string part where the error happened
  std::string const region(queryString().extractRegion(line, column));

  // note: line numbers reported by bison/flex start at 1, columns start at 0
  std::stringstream errorMessage;
  errorMessage << data << std::string(" near '") << region
               << std::string("' at position ") << line << std::string(":")
               << (column + 1);

  if (_query.queryOptions().verboseErrors) {
    errorMessage << std::endl << queryString() << std::endl;

    // create a neat pointer to the location of the error.
    size_t i;
    for (i = 0; i + 1 < (size_t)column; i++) {
      errorMessage << ' ';
    }
    if (i > 0) {
      errorMessage << '^';
    }
    errorMessage << '^' << '^' << std::endl;
  }

  _query.warnings().registerError(errorCode, errorMessage.str());
}

/// @brief register a warning
void Parser::registerWarning(ErrorCode errorCode, std::string_view data,
                             [[maybe_unused]] int line, [[maybe_unused]] int column) {
  // ignore line and column for now
  _query.warnings().registerWarning(errorCode, data);
}

/// @brief push an AstNode array element on top of the stack
/// the array must be removed from the stack via popArray
void Parser::pushArray(AstNode* array) {
  TRI_ASSERT(array != nullptr);
  TRI_ASSERT(array->type == NODE_TYPE_ARRAY);
  array->setFlag(DETERMINED_CONSTANT, VALUE_CONSTANT);
  pushStack(array);
}

/// @brief pop an array value from the parser's stack
/// the array must have been added to the stack via pushArray
AstNode* Parser::popArray() {
  AstNode* array = static_cast<AstNode*>(popStack());
  TRI_ASSERT(array->type == NODE_TYPE_ARRAY);
  return array;
}

/// @brief push an AstNode into the array element on top of the stack
void Parser::pushArrayElement(AstNode* node) {
  TRI_ASSERT(node != nullptr);
  auto array = static_cast<AstNode*>(peekStack());
  TRI_ASSERT(array->type == NODE_TYPE_ARRAY);
  array->addMember(node);
  if (array->hasFlag(AstNodeFlagType::VALUE_CONSTANT) && !node->isConstant()) {
    array->removeFlag(AstNodeFlagType::VALUE_CONSTANT);
  }
}

/// @brief push an AstNode into the object element on top of the stack
void Parser::pushObjectElement(char const* attributeName, size_t nameLength, AstNode* node) {
  auto object = static_cast<AstNode*>(peekStack());
  TRI_ASSERT(object != nullptr);
  TRI_ASSERT(object->type == NODE_TYPE_OBJECT);
  auto element = _ast.createNodeObjectElement(attributeName, nameLength, node);
  object->addMember(element);
}

/// @brief push an AstNode into the object element on top of the stack
void Parser::pushObjectElement(AstNode* attributeName, AstNode* node) {
  auto object = static_cast<AstNode*>(peekStack());
  TRI_ASSERT(object != nullptr);
  TRI_ASSERT(object->type == NODE_TYPE_OBJECT);
  auto element = _ast.createNodeCalculatedObjectElement(attributeName, node);
  object->addMember(element);
}

/// @brief push a temporary value on the parser's stack
void Parser::pushStack(void* value) {
  TRI_ASSERT(value != nullptr);
  _stack.emplace_back(value); 
}

/// @brief pop a temporary value from the parser's stack
void* Parser::popStack() {
  TRI_ASSERT(!_stack.empty());

  void* result = _stack.back();
  TRI_ASSERT(result != nullptr);
  _stack.pop_back();
  return result;
}

/// @brief peek at a temporary value from the parser's stack
void* Parser::peekStack() {
  TRI_ASSERT(!_stack.empty());

  return _stack.back();
}
