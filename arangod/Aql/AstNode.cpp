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
#include "Aql/Scopes.h"

using namespace triagens::aql;

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
/// @brief destroy the node
////////////////////////////////////////////////////////////////////////////////

AstNode::~AstNode () {
  TRI_DestroyVectorPointer(&members);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the node into JSON
////////////////////////////////////////////////////////////////////////////////

void AstNode::toJson (TRI_json_t* json,
                      TRI_memory_zone_t* zone) {
  TRI_ASSERT(TRI_IsListJson(json));

  TRI_json_t* node = TRI_CreateArrayJson(zone);

  if (node == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // dump node type
  TRI_Insert3ArrayJson(zone, node, "type", TRI_CreateStringCopyJson(zone, typeString().c_str()));

  if (type == NODE_TYPE_REFERENCE ||
      type == NODE_TYPE_COLLECTION ||
      type == NODE_TYPE_PARAMETER ||
      type == NODE_TYPE_ATTRIBUTE_ACCESS ||
      type == NODE_TYPE_FCALL ||
      type == NODE_TYPE_FCALL_USER) {
    // dump "name" of node
    TRI_Insert3ArrayJson(zone, node, "name", TRI_CreateStringCopyJson(zone, getStringValue()));
  }

  if (type == NODE_TYPE_VALUE) {
    // dump value of "value" node
    switch (value.type) {
      case VALUE_TYPE_NULL:
        TRI_Insert3ArrayJson(zone, node, "value", TRI_CreateStringCopyJson(zone, "null"));
        break;
      case VALUE_TYPE_BOOL:
        TRI_Insert3ArrayJson(zone, node, "value", TRI_CreateStringCopyJson(zone, value.value._bool ? "true" : "false"));
        break;
      case VALUE_TYPE_INT:
        TRI_Insert3ArrayJson(zone, node, "value", TRI_CreateNumberJson(zone, static_cast<double>(value.value._int)));
        break;
      case VALUE_TYPE_DOUBLE:
        TRI_Insert3ArrayJson(zone, node, "value", TRI_CreateNumberJson(zone, value.value._double));
        break;
      case VALUE_TYPE_STRING:
        TRI_Insert3ArrayJson(zone, node, "value", TRI_CreateStringCopyJson(zone, value.value._string));
        break;
      default: {
      }
    }
  }

  if (type == NODE_TYPE_VARIABLE) {
    auto variable = static_cast<Variable*>(getData());

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
        if (member != nullptr) {
          member->toJson(subNodes, zone);
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

  TRI_PushBack3ListJson(zone, json, node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type name of a node
////////////////////////////////////////////////////////////////////////////////

std::string AstNode::typeString () const {
  switch (type) {
    case NODE_TYPE_ROOT:
      return "root";
    case NODE_TYPE_FOR:
      return "for";
    case NODE_TYPE_LET:
      return "let";
    case NODE_TYPE_FILTER:
      return "filter";
    case NODE_TYPE_RETURN:
      return "return";
    case NODE_TYPE_REMOVE:
      return "remove";
    case NODE_TYPE_INSERT:
      return "insert";
    case NODE_TYPE_UPDATE:
      return "update";
    case NODE_TYPE_REPLACE:
      return "replace";
    case NODE_TYPE_COLLECT:
      return "collect";
    case NODE_TYPE_SORT:
      return "sort";
    case NODE_TYPE_SORT_ELEMENT:
      return "sort element";
    case NODE_TYPE_LIMIT:
      return "limit";
    case NODE_TYPE_VARIABLE:
      return "variable";
    case NODE_TYPE_ASSIGN:
      return "assign";
    case NODE_TYPE_OPERATOR_UNARY_PLUS:
      return "unary plus";
    case NODE_TYPE_OPERATOR_UNARY_MINUS:
      return "unary minus";
    case NODE_TYPE_OPERATOR_UNARY_NOT:
      return "unary not";
    case NODE_TYPE_OPERATOR_BINARY_AND:
      return "logical and";
    case NODE_TYPE_OPERATOR_BINARY_OR:
      return "logical or";
    case NODE_TYPE_OPERATOR_BINARY_PLUS:
      return "plus";
    case NODE_TYPE_OPERATOR_BINARY_MINUS:
      return "minus";
    case NODE_TYPE_OPERATOR_BINARY_TIMES:
      return "times";
    case NODE_TYPE_OPERATOR_BINARY_DIV:
      return "division";
    case NODE_TYPE_OPERATOR_BINARY_MOD:
      return "modulus";
    case NODE_TYPE_OPERATOR_BINARY_EQ:
      return "compare ==";
    case NODE_TYPE_OPERATOR_BINARY_NE:
      return "compare !=";
    case NODE_TYPE_OPERATOR_BINARY_LT:
      return "compare <";
    case NODE_TYPE_OPERATOR_BINARY_LE:
      return "compare <=";
    case NODE_TYPE_OPERATOR_BINARY_GT:
      return "compare >";
    case NODE_TYPE_OPERATOR_BINARY_GE:
      return "compare >=";
    case NODE_TYPE_OPERATOR_BINARY_IN:
      return "compare in";
    case NODE_TYPE_OPERATOR_TERNARY:
      return "ternary";
    case NODE_TYPE_SUBQUERY:
      return "subquery";
    case NODE_TYPE_ATTRIBUTE_ACCESS:
      return "attribute access";
    case NODE_TYPE_BOUND_ATTRIBUTE_ACCESS:
      return "bound attribute access";
    case NODE_TYPE_INDEXED_ACCESS:
      return "indexed access";
    case NODE_TYPE_EXPAND:
      return "expand";
    case NODE_TYPE_VALUE:
      return "value";
    case NODE_TYPE_LIST:
      return "list";
    case NODE_TYPE_ARRAY:
      return "array";
    case NODE_TYPE_ARRAY_ELEMENT:
      return "array element";
    case NODE_TYPE_COLLECTION:
      return "collection";
    case NODE_TYPE_REFERENCE:
      return "reference";
    case NODE_TYPE_ATTRIBUTE:
      return "attribute";
    case NODE_TYPE_PARAMETER:
      return "parameter";
    case NODE_TYPE_FCALL:
      return "function call";
    case NODE_TYPE_FCALL_USER:
      return "user function call";
    default: {
    }
  }

  TRI_ASSERT(false);
  return "";
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
