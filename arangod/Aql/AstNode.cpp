////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, AST node
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

#include "Aql/AstNode.h"
#include "Aql/Ast.h"
#include "Aql/Function.h"
#include "Aql/Scopes.h"
#include "Aql/V8Executor.h"
#include "Basics/StringBuffer.h"
#include "Basics/JsonHelper.h"

using namespace triagens::aql;
using JsonHelper = triagens::basics::JsonHelper;
using Json = triagens::basics::Json;

std::unordered_map<int, std::string const> const AstNode::TypeNames{ 
    { static_cast<int>(NODE_TYPE_ROOT),                     "root" },
    { static_cast<int>(NODE_TYPE_FOR),                      "for" },
    { static_cast<int>(NODE_TYPE_LET),                      "let" },
    { static_cast<int>(NODE_TYPE_FILTER),                   "filter" },
    { static_cast<int>(NODE_TYPE_RETURN),                   "return" },
    { static_cast<int>(NODE_TYPE_REMOVE),                   "remove" },
    { static_cast<int>(NODE_TYPE_INSERT),                   "insert" },
    { static_cast<int>(NODE_TYPE_UPDATE),                   "update" },
    { static_cast<int>(NODE_TYPE_REPLACE),                  "replace" },
    { static_cast<int>(NODE_TYPE_COLLECT),                  "collect" },
    { static_cast<int>(NODE_TYPE_SORT),                     "sort" },
    { static_cast<int>(NODE_TYPE_SORT_ELEMENT),             "sort element" },
    { static_cast<int>(NODE_TYPE_LIMIT),                    "limit" },
    { static_cast<int>(NODE_TYPE_VARIABLE),                 "variable" },
    { static_cast<int>(NODE_TYPE_ASSIGN),                   "assign" },
    { static_cast<int>(NODE_TYPE_OPERATOR_UNARY_PLUS),      "unary plus" },
    { static_cast<int>(NODE_TYPE_OPERATOR_UNARY_MINUS),     "unary minus" },
    { static_cast<int>(NODE_TYPE_OPERATOR_UNARY_NOT),       "unary not" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_AND),      "logical and" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_OR),       "logical or" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_PLUS),     "plus" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MINUS),    "minus" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_TIMES),    "times" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_DIV),      "division" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_MOD),      "modulus" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_EQ),       "compare ==" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_NE),       "compare !=" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LT),       "compare <" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_LE),       "compare <=" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GT),       "compare >" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_GE),       "compare >=" },
    { static_cast<int>(NODE_TYPE_OPERATOR_BINARY_IN),       "compare in" },
    { static_cast<int>(NODE_TYPE_OPERATOR_TERNARY),         "ternary" },
    { static_cast<int>(NODE_TYPE_SUBQUERY),                 "subquery" },
    { static_cast<int>(NODE_TYPE_ATTRIBUTE_ACCESS),         "attribute access" },
    { static_cast<int>(NODE_TYPE_BOUND_ATTRIBUTE_ACCESS),   "bound attribute access" },
    { static_cast<int>(NODE_TYPE_INDEXED_ACCESS),           "indexed access" },
    { static_cast<int>(NODE_TYPE_EXPAND),                   "expand" },
    { static_cast<int>(NODE_TYPE_ITERATOR),                 "iterator" },
    { static_cast<int>(NODE_TYPE_VALUE),                    "value" },
    { static_cast<int>(NODE_TYPE_LIST),                     "list" },
    { static_cast<int>(NODE_TYPE_ARRAY),                    "array" },
    { static_cast<int>(NODE_TYPE_ARRAY_ELEMENT),            "array element" },
    { static_cast<int>(NODE_TYPE_COLLECTION),               "collection" },
    { static_cast<int>(NODE_TYPE_REFERENCE),                "reference" },
    { static_cast<int>(NODE_TYPE_PARAMETER),                "parameter" },
    { static_cast<int>(NODE_TYPE_FCALL),                    "function call" },
    { static_cast<int>(NODE_TYPE_FCALL_USER),               "user function call" },
    { static_cast<int>(NODE_TYPE_RANGE),                    "range" },
    { static_cast<int>(NODE_TYPE_NOP),                      "no-op" }
};

std::unordered_map<int, std::string const> const AstNode::valueTypeNames{
    { static_cast<int>(VALUE_TYPE_NULL),                    "null" },
    { static_cast<int>(VALUE_TYPE_BOOL),                    "bool" },
    { static_cast<int>(VALUE_TYPE_INT),                     "int" },
    { static_cast<int>(VALUE_TYPE_DOUBLE),                  "double" },
    { static_cast<int>(VALUE_TYPE_STRING),                  "string" }
};

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the node
////////////////////////////////////////////////////////////////////////////////

AstNode::AstNode (AstNodeType type)
  : type(type) {

  TRI_InitVectorPointer(&members, TRI_UNKNOWN_MEM_ZONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the node from JSON
////////////////////////////////////////////////////////////////////////////////

AstNode::AstNode (Ast* ast,
                  triagens::basics::Json const& json) 
  : type(getNodeTypeFromJson(json)) {

  auto query = ast->query();
  TRI_InitVectorPointer(&members, TRI_UNKNOWN_MEM_ZONE);

  switch (type) {
    case NODE_TYPE_COLLECTION:
    case NODE_TYPE_PARAMETER:
    case NODE_TYPE_ATTRIBUTE_ACCESS:
    case NODE_TYPE_FCALL_USER:
      value.type = VALUE_TYPE_STRING;
      setStringValue(query->registerString(JsonHelper::getStringValue(json.json(),
                                                                      "name", ""),
                                           false));
      break;
    case NODE_TYPE_VALUE: {
      int vType = JsonHelper::checkAndGetNumericValue<int>(json.json(), "vTypeID");
      validateValueType(vType);
      value.type = static_cast<AstNodeValueType>(vType);

      switch (value.type) {
        case VALUE_TYPE_NULL:
          break;
        case VALUE_TYPE_BOOL:
          value.value._bool = JsonHelper::checkAndGetBooleanValue(json.json(), "value");
          break;
        case VALUE_TYPE_INT:
          setIntValue(JsonHelper::checkAndGetNumericValue<int64_t>(json.json(), "value"));
          break;
        case VALUE_TYPE_DOUBLE:
          setDoubleValue(JsonHelper::checkAndGetNumericValue<double>(json.json(), "value"));
          break;
        case VALUE_TYPE_STRING:
          setStringValue(query->registerString(JsonHelper::checkAndGetStringValue(json.json(), "value"), false));
          break;
        default: {
        }
      }
      break;
    }
    case NODE_TYPE_VARIABLE: {
      auto variable = ast->variables()->createVariable(json);
      TRI_ASSERT(variable != nullptr);
      setData(variable);
      break;
    }
    case NODE_TYPE_REFERENCE: {
      auto variableId = JsonHelper::checkAndGetNumericValue<VariableId>(json.json(), "id");
      auto variable = ast->variables()->getVariable(variableId);

      TRI_ASSERT(variable != nullptr);
      setData(variable);
      break;
    }
    case NODE_TYPE_FCALL: {
      setData(query->executor()->getFunctionByName(JsonHelper::getStringValue(json.json(), "name", "")));
      break;
    }
    case NODE_TYPE_ARRAY_ELEMENT: {
      setStringValue(query->registerString(JsonHelper::getStringValue(json.json(),
                                                                      "name", ""),
                                           false));
      break;
    }
    case NODE_TYPE_ARRAY:
    case NODE_TYPE_ROOT:
    case NODE_TYPE_FOR:
    case NODE_TYPE_LET:
    case NODE_TYPE_FILTER:
    case NODE_TYPE_RETURN:
    case NODE_TYPE_REMOVE:
    case NODE_TYPE_INSERT:
    case NODE_TYPE_UPDATE:
    case NODE_TYPE_REPLACE:
    case NODE_TYPE_COLLECT:
    case NODE_TYPE_SORT:
    case NODE_TYPE_SORT_ELEMENT:
    case NODE_TYPE_LIMIT:
    case NODE_TYPE_ASSIGN:
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
    case NODE_TYPE_OPERATOR_UNARY_NOT:
    case NODE_TYPE_OPERATOR_BINARY_AND:
    case NODE_TYPE_OPERATOR_BINARY_OR:
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
    case NODE_TYPE_OPERATOR_BINARY_DIV:
    case NODE_TYPE_OPERATOR_BINARY_MOD:
    case NODE_TYPE_OPERATOR_BINARY_EQ:
    case NODE_TYPE_OPERATOR_BINARY_NE:
    case NODE_TYPE_OPERATOR_BINARY_LT:
    case NODE_TYPE_OPERATOR_BINARY_LE:
    case NODE_TYPE_OPERATOR_BINARY_GT:
    case NODE_TYPE_OPERATOR_BINARY_GE:
    case NODE_TYPE_OPERATOR_BINARY_IN:
    case NODE_TYPE_OPERATOR_TERNARY:
    case NODE_TYPE_SUBQUERY:
    case NODE_TYPE_BOUND_ATTRIBUTE_ACCESS:
    case NODE_TYPE_INDEXED_ACCESS:
    case NODE_TYPE_EXPAND:
    case NODE_TYPE_ITERATOR:
    case NODE_TYPE_LIST:
    case NODE_TYPE_RANGE:
    case NODE_TYPE_NOP:
      break;
    }

  Json subNodes = json.get("subNodes");
  if (subNodes.isList()) {
    size_t const len = subNodes.size();
    for (size_t i = 0; i < len; i++) {
      Json subNode = subNodes.at(i);
      addMember(new AstNode(ast, subNode));
    }
  }

  ast->addNode(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the node
////////////////////////////////////////////////////////////////////////////////

AstNode::~AstNode () {
  TRI_DestroyVectorPointer(&members);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type name of a node
////////////////////////////////////////////////////////////////////////////////

std::string const& AstNode::getTypeString () const {
  auto it = TypeNames.find(static_cast<int>(type));
  if (it != TypeNames.end()) {
    return (*it).second;
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "missing node type in TypeNames");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the value type name of a node
////////////////////////////////////////////////////////////////////////////////

std::string const& AstNode::getValueTypeString () const {
  auto it = valueTypeNames.find(static_cast<int>(value.type));
  if (it != valueTypeNames.end()) {
    return (*it).second;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "missing node type in valueTypeNames");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a type of this kind; throws exception if not.
////////////////////////////////////////////////////////////////////////////////

void AstNode::validateType (int type) {
  auto it = TypeNames.find(static_cast<int>(type));
  if (it == TypeNames.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "unknown AST-Node TypeID");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether we know a value type of this kind; 
/// throws exception if not.
////////////////////////////////////////////////////////////////////////////////

void AstNode::validateValueType (int type) {
  auto it = valueTypeNames.find(static_cast<int>(type));
  if (it == valueTypeNames.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "invalid AST-Node valueTypeName");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a node's type from json
////////////////////////////////////////////////////////////////////////////////

AstNodeType AstNode::getNodeTypeFromJson (triagens::basics::Json const& json) {
  int type = JsonHelper::checkAndGetNumericValue<int>(json.json(), "typeID");
  validateType (type);
  return (AstNodeType) type;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the node value
/// the caller is responsible for freeing the JSON later
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* AstNode::toJsonValue (TRI_memory_zone_t* zone) const {
  if (type == NODE_TYPE_VALUE) {
    // dump value of "value" node
    switch (value.type) {
      case VALUE_TYPE_NULL:
        return TRI_CreateNullJson(zone);
      case VALUE_TYPE_BOOL:
        return TRI_CreateBooleanJson(zone, value.value._bool);
      case VALUE_TYPE_INT:
        return TRI_CreateNumberJson(zone, static_cast<double>(value.value._int));
      case VALUE_TYPE_DOUBLE:
        return TRI_CreateNumberJson(zone, value.value._double);
      case VALUE_TYPE_STRING:
        return TRI_CreateStringCopyJson(zone, value.value._string);
      default: {
      }
    }
  }
  
  if (type == NODE_TYPE_LIST) {
    size_t const n = numMembers();
    TRI_json_t* list = TRI_CreateList2Json(zone, n);

    if (list == nullptr) {
      return nullptr;
    }

    for (size_t i = 0; i < n; ++i) {
      auto member = getMember(i);

      if (member != nullptr) {
        TRI_json_t* j = member->toJsonValue(zone);

        if (j != nullptr) {
          TRI_PushBack3ListJson(zone, list, j);
        }
      }
    }
    return list;
  }
  
  if (type == NODE_TYPE_ARRAY) {
    size_t const n = numMembers();
    TRI_json_t* array = TRI_CreateArray2Json(zone, n);

    for (size_t i = 0; i < n; ++i) {
      auto member = getMember(i);

      if (member != nullptr) {
        TRI_json_t* j = member->getMember(0)->toJsonValue(zone);

        if (j != nullptr) {
          TRI_Insert3ArrayJson(zone, array, member->getStringValue(), j);
        }
      }
    }

    return array;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the node
/// the caller is responsible for freeing the JSON later
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* AstNode::toJson (TRI_memory_zone_t* zone,
                             bool verbose) const {
  TRI_json_t* node = TRI_CreateArrayJson(zone);

  if (node == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // dump node type
  TRI_Insert3ArrayJson(zone, node, "type", TRI_CreateStringCopyJson(zone, getTypeString().c_str()));
  if (verbose) {
    TRI_Insert3ArrayJson(zone, node, "typeID", TRI_CreateNumberJson(zone, static_cast<int>(type)));
  }

  if (type == NODE_TYPE_COLLECTION ||
      type == NODE_TYPE_PARAMETER ||
      type == NODE_TYPE_ATTRIBUTE_ACCESS ||
      type == NODE_TYPE_ARRAY_ELEMENT ||
      type == NODE_TYPE_FCALL_USER) {
    // dump "name" of node
    TRI_Insert3ArrayJson(zone, node, "name", TRI_CreateStringCopyJson(zone, getStringValue()));
  }

  if (type == NODE_TYPE_FCALL) {
    auto func = static_cast<Function*>(getData());
    TRI_Insert3ArrayJson(zone, node, "name", TRI_CreateStringCopyJson(zone, func->externalName.c_str()));
    // arguments are exported via node members
  }

  if (type == NODE_TYPE_VALUE) {
    // dump value of "value" node
    auto v = toJsonValue(zone);

    if (v == nullptr) {
      TRI_FreeJson(zone, node);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    
    TRI_Insert3ArrayJson(zone, node, "value", v);
    if (verbose) {
      TRI_Insert3ArrayJson(zone, node, "vType", TRI_CreateStringCopyJson(zone, getValueTypeString().c_str()));
      TRI_Insert3ArrayJson(zone, node, "vTypeID", TRI_CreateNumberJson(zone, static_cast<int>(value.type)));
    }
  }

  if (type == NODE_TYPE_VARIABLE ||
      type == NODE_TYPE_REFERENCE) {
    auto variable = static_cast<Variable*>(getData());

    TRI_ASSERT(variable != nullptr);

    /// TODO: use variable.toJson()!!!
    TRI_Insert3ArrayJson(zone, node, "name", TRI_CreateStringCopyJson(zone, variable->name.c_str()));
    TRI_Insert3ArrayJson(zone, node, "id", TRI_CreateNumberJson(zone, static_cast<double>(variable->id)));
  }
  
  // dump sub-nodes 
  size_t const n = members._length;

  if (n > 0) {
    TRI_json_t* subNodes = TRI_CreateListJson(zone);

    if (subNodes == nullptr) {
      TRI_FreeJson(zone, node);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    try {
      for (size_t i = 0; i < n; ++i) {
        auto member = getMember(i);
        if (member != nullptr && member->type != NODE_TYPE_NOP) {
          member->toJson(subNodes, zone, verbose);
        }
      }
    }
    catch (...) {
      TRI_FreeJson(zone, subNodes);
      TRI_FreeJson(zone, node);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    
    TRI_Insert3ArrayJson(zone, node, "subNodes", subNodes);
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a JSON representation of the node to the JSON list specified
/// in the first argument
////////////////////////////////////////////////////////////////////////////////

void AstNode::toJson (TRI_json_t* json,
                      TRI_memory_zone_t* zone,
                      bool verbose) const {
  TRI_ASSERT(TRI_IsListJson(json));

  TRI_json_t* node = toJson(zone, verbose);

  if (node == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_PushBack3ListJson(zone, json, node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the node's value to a boolean value
////////////////////////////////////////////////////////////////////////////////

bool AstNode::toBoolean () const {
  if (type == NODE_TYPE_VALUE) {
    switch (value.type) {
      case VALUE_TYPE_BOOL: {
        return value.value._bool;
      }
      case VALUE_TYPE_INT: {
        return (value.value._int != 0);
      }
      case VALUE_TYPE_DOUBLE: {
        return value.value._double != 0.0;
      }
      case VALUE_TYPE_STRING: {
        return (*value.value._string != '\0');
      }
      case VALUE_TYPE_NULL: 
      case VALUE_TYPE_FAIL: {
        return false;
      }
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node is simple enough to be used in a simple
/// expression
////////////////////////////////////////////////////////////////////////////////

bool AstNode::isSimple () const {
  if (type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    TRI_ASSERT(numMembers() == 1);
    return getMember(0)->isSimple();
  }

  if (type == NODE_TYPE_INDEXED_ACCESS) {
    TRI_ASSERT(numMembers() == 2);
    return (getMember(0)->isSimple() && getMember(1)->isSimple());
  }

  if (type == NODE_TYPE_REFERENCE) {
    return true;
  }

  if (type == NODE_TYPE_LIST) {
    size_t const n = numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto member = getMember(i);
      if (! member->isSimple()) {
        return false;
      }
    }
    return true;
  }

  if (type == NODE_TYPE_ARRAY) {
    size_t const n = numMembers();
    for (size_t i = 0; i < n; ++i) {
      auto member = getMember(i);
      if (! member->isSimple()) {
        return false;
      }
    }
    return true;
  }

  if (type == NODE_TYPE_ARRAY_ELEMENT) {
    auto member = getMember(0);
    return member->isSimple();
  }
  
  if (type == NODE_TYPE_VALUE) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node has a constant value
////////////////////////////////////////////////////////////////////////////////

bool AstNode::isConstant () const {
  if (type == NODE_TYPE_VALUE) {
    return true;
  }

  if (type == NODE_TYPE_LIST) {
    size_t const n = numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = getMember(i);

      if (member != nullptr) {
        if (! member->isConstant()) {
          return false;
        }
      }
    }

    return true;
  }

  if (type == NODE_TYPE_ARRAY) {
    size_t const n = numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = getMember(i);

      if (member != nullptr) {
        auto value = member->getMember(0);

        if (value == nullptr) {
          continue;
        }

        if (! value->isConstant()) {
          return false;
        }
      }
    }

    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a node (and its subnodes) can throw a runtime
/// exception
////////////////////////////////////////////////////////////////////////////////

bool AstNode::canThrow () const {
  // check sub-nodes first
  size_t const n = numMembers();
  for (size_t i = 0; i < n; ++i) {
    auto member = getMember(i);
    if (member->canThrow()) {
      // if any sub-node may throw, the whole branch may throw
      return true;
    }
  }

  // no sub-node throws, now check ourselves

  if (type == NODE_TYPE_OPERATOR_UNARY_PLUS ||
      type == NODE_TYPE_OPERATOR_UNARY_MINUS ||
      type == NODE_TYPE_OPERATOR_UNARY_NOT) {
    // all unary operators may throw
    return true;
  }

  if (type == NODE_TYPE_OPERATOR_BINARY_AND ||
      type == NODE_TYPE_OPERATOR_BINARY_OR) {
    // the logical operators can throw
    return true;
  }

  if (type == NODE_TYPE_OPERATOR_BINARY_PLUS ||
      type == NODE_TYPE_OPERATOR_BINARY_MINUS ||
      type == NODE_TYPE_OPERATOR_BINARY_TIMES ||
      type == NODE_TYPE_OPERATOR_BINARY_DIV ||
      type == NODE_TYPE_OPERATOR_BINARY_MOD) {
    // the arithmetic operators can throw
    return true;
  }

  if (type == NODE_TYPE_OPERATOR_BINARY_IN) {
    // the IN operator can throw (if rhs is not a list)
    return true;
  }
  
  if (type == NODE_TYPE_OPERATOR_TERNARY) {
    return true;
  }

  if (type == NODE_TYPE_INDEXED_ACCESS) {
    // TODO: validate whether this can actually throw
    return true;
  }

  if (type == NODE_TYPE_EXPAND) {
    // TODO: validate whether this can actually throw
    return true;
  }
  
  if (type == NODE_TYPE_FCALL) {
    // built-in functions may or may not throw
    auto func = static_cast<Function*>(getData());
    return func->canThrow;
  }
  
  if (type == NODE_TYPE_FCALL_USER) {
    // user functions can always throw
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a JavaScript representation of the node into a string buffer
////////////////////////////////////////////////////////////////////////////////

void AstNode::append (triagens::basics::StringBuffer* buffer) const {
  if (type == NODE_TYPE_VALUE) {
    appendValue(buffer);
    return;
  }

  if (type == NODE_TYPE_LIST) {
    buffer->appendText("[ ");
    size_t const n = numMembers();
    for (size_t i = 0; i < n; ++i) {
      if (i > 0) {
        buffer->appendText(", ");
      }

      AstNode* member = getMember(i);
      if (member != nullptr) {
        member->append(buffer);
      }
    }
    buffer->appendText(" ]");
    return;
  }

  if (type == NODE_TYPE_ARRAY) {
    buffer->appendText("{ ");
    size_t const n = numMembers();
    for (size_t i = 0; i < n; ++i) {
      if (i > 0) {
        buffer->appendText(", ");
      }

      AstNode* member = getMember(i);
      if (member != nullptr) {
        TRI_ASSERT(member->type == NODE_TYPE_ARRAY_ELEMENT);
        TRI_ASSERT(member->numMembers() == 1);

        buffer->appendChar('"');
        buffer->appendJsonEncoded(member->getStringValue());
        buffer->appendText("\" : ");

        member->getMember(0)->append(buffer);
      }
    }
    buffer->appendText(" }");
    return;
  }

  if (type == NODE_TYPE_OPERATOR_BINARY_AND ||
      type == NODE_TYPE_OPERATOR_BINARY_OR ||
      type == NODE_TYPE_OPERATOR_BINARY_PLUS ||
      type == NODE_TYPE_OPERATOR_BINARY_MINUS ||
      type == NODE_TYPE_OPERATOR_BINARY_TIMES ||
      type == NODE_TYPE_OPERATOR_BINARY_DIV ||
      type == NODE_TYPE_OPERATOR_BINARY_MOD ||
      type == NODE_TYPE_OPERATOR_BINARY_EQ ||
      type == NODE_TYPE_OPERATOR_BINARY_NE ||
      type == NODE_TYPE_OPERATOR_BINARY_LT ||
      type == NODE_TYPE_OPERATOR_BINARY_LE ||
      type == NODE_TYPE_OPERATOR_BINARY_GT ||
      type == NODE_TYPE_OPERATOR_BINARY_GE ||
      type == NODE_TYPE_OPERATOR_BINARY_IN) {
    size_t const n = numMembers();
    TRI_ASSERT(n == 2);

    getMember(0)->append(buffer);

    buffer->appendText(", ");

    getMember(1)->append(buffer);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify the value of a node into a string buffer
////////////////////////////////////////////////////////////////////////////////

void AstNode::appendValue (triagens::basics::StringBuffer* buffer) const {
  TRI_ASSERT(type == NODE_TYPE_VALUE);

  switch (value.type) {
    case VALUE_TYPE_BOOL: {
      if (value.value._bool) {
        buffer->appendText("true", 4);
      }
      else {
        buffer->appendText("false", 5);
      }
      break;
    }

    case VALUE_TYPE_INT: {
      buffer->appendInteger(value.value._int);
      break;
    }

    case VALUE_TYPE_DOUBLE: {
      buffer->appendDecimal(value.value._double);
      break;
    }

    case VALUE_TYPE_STRING: {
      buffer->appendChar('"');
      buffer->appendJsonEncoded(value.value._string);
      buffer->appendChar('"');
      break;
    }

    case VALUE_TYPE_NULL: 
    default: {
      buffer->appendText("null", 4);
      break;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
