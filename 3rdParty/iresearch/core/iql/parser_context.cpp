////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "parser_context.hpp"

#ifndef _MSC_VER
#include <string.h>
#endif
#include <stdint.h>

#include <absl/container/flat_hash_set.h>

#include <frozen/unordered_map.h>
#include <frozen/string.h>

using namespace iresearch::iql;

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

namespace {
constexpr parser::semantic_type UNKNOWN = 0; // no known value
constexpr parser::semantic_type TRUE = 1; // expression evaluating to true
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

parser_context::parser_context(
  std::string const& sData, functions const& functions /*= defaults::FUNCTIONS*/
): m_sData(sData), m_functions(functions), m_nNext(0), m_eState(StateType::NONE)
{
  m_nodes.resize(2); // add an error node at position 0 (a.k.a. UNKNOWN)

  // initialize 'BOOL_TRUE' at position 1
  m_nodes[1].type = query_node::NodeType::BOOL_TRUE;

  m_error.first = false;
  m_filter.first = false;
  m_limit.first = false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 parser operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief error printer
///        NOTE: this method is not triggered if all GLR states collapse to fail
////////////////////////////////////////////////////////////////////////////////
void parser_context::yyerror(
  parser::location_type const& location, std::string const& sError
) {
  m_error.first = true;
  m_error.second.sMessage = sError;
  m_error.second.nStart = location.begin.column;
  m_error.second.nEnd = location.end.column;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lexical analyzer
///        The semantic value of the token (if it has one) is stored into the
///        variable 'value', the position of the token is stored into the
///        variable 'location' with fields:
///        begin.line, begin.column, end.line, end.column
/// @return numeric code which represents a token type
///         A token type code of zero is returned if the end-of-input is
///         encountered
////////////////////////////////////////////////////////////////////////////////
parser::token_type parser_context::yylex(
  parser::semantic_type& value, parser::location_type& location
) {
  location.begin.column = (decltype(location.begin.column))m_nNext;

  parser::token_type type = next();

  location.end.column = (decltype(location.begin.column))m_nNext;
  value = UNKNOWN; // reset to undefined

  return type;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  value operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a node representing a sequence literal
/// @return ID of the node with the literal or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::sequence(
  parser::location_type const& location
) {
  if (location.end.column < location.begin.column ||
      m_sData.size() < static_cast<size_t>(location.end.column)) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN); // index out of bounds
  }

  parser::semantic_type value;
  auto& node = create_node(value);

  node.type = query_node::NodeType::SEQUENCE;
  node.sValue = m_sData.substr(
    location.begin.column, location.end.column - location.begin.column
  );

  return value; // ID of new node
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   node operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief append a value to a node
/// @return ID of the node with appended value or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::append(
  parser::semantic_type const& value, parser::location_type const& location
) {
  auto& node = find_node(value);

  if (query_node::NodeType::SEQUENCE != node.type) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN);
  }

  std::string sValue = m_sData.substr(
    location.begin.column, location.end.column - location.begin.column
  );

  node.sValue.append(sValue);

  return value; // ID of modified node
}

////////////////////////////////////////////////////////////////////////////////
/// @brief boost node rank by the specified value
/// @return ID of the boosted node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::boost(
  parser::semantic_type const& value, parser::location_type const& location
) {
  auto& node = find_node(value);
  char const* pcStart = &(m_sData.c_str()[location.begin.column]);
  char* pcNext;
  float fValue = strtof(pcStart, &pcNext);

  if (pcNext - pcStart != location.end.column - location.begin.column) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN);
  }

  node.fBoost *= fValue;

  return value; // ID of modified node
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a node representing a function call
/// @return ID of the node with the function call or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::function(
  parser::semantic_type const& name, parser::semantic_type const& args
) {
  auto& nameNode = find_node(name);
  auto& argsNode = find_node(args);
  size_t nArgsCount;
  bool bArgsDirect = false;

  if (query_node::NodeType::SEQUENCE != nameNode.type) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN); // invalid name
  }

  switch (argsNode.type) {
  case query_node::NodeType::LIST:
    nArgsCount = argsNode.children.size();
    break;
  case query_node::NodeType::FUNCTION:
    if (!argsNode.pFnBoolean && !argsNode.pFnSequence) {
      return *const_cast<parser::semantic_type*>(&UNKNOWN); // invalid args
    }
    // fall through
  case query_node::NodeType::UNION: // fall through
  case query_node::NodeType::INTERSECTION: // fall through
  case query_node::NodeType::BOOL_TRUE: // fall through
  case query_node::NodeType::EQUAL: // fall through
  case query_node::NodeType::LIKE: // fall through
  case query_node::NodeType::SEQUENCE:
    nArgsCount = 1;
    bArgsDirect = true;
    break;
  case query_node::NodeType::UNKNOWN:
    nArgsCount = 0;
    break;
  default:
    return *const_cast<parser::semantic_type*>(&UNKNOWN); // invalid args
  }

  // function type depends on the parent node and can be any of the following
  auto* pBestBoolFn =
    find_best_function(nameNode.sValue, nArgsCount, m_functions.boolFns);
  auto* pBestOrderFn =
    find_best_function(nameNode.sValue, nArgsCount, m_functions.orderFns);
  auto* pBestSeqFn =
    find_best_function(nameNode.sValue, nArgsCount, m_functions.seqFns);

  if (!pBestBoolFn && !pBestOrderFn && !pBestSeqFn) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN); // unknown fn
  }

  parser::semantic_type value;
  auto& node = create_node(value);

  if (bArgsDirect) {
    node.children.emplace_back(args);
  }
  else {
    // after call to creat_node(...) 'nameNode' and 'argsNode' values are undefined
    node.children = find_node(args).children; // get new reference to args node
  }

  node.pFnBoolean = pBestBoolFn;
  node.pFnOrder = pBestOrderFn;
  node.pFnSequence = pBestSeqFn;
  node.type = query_node::NodeType::FUNCTION;
  node.sValue = nameNode.sValue;

  return value; // ID of new node
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append a specified node to the specified list
/// @return ID of the list node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::list(
  parser::semantic_type const& value1, parser::semantic_type const& value2
) {
  auto& node1 = find_node(value1);
  auto& node2 = find_node(value2);

  if (query_node::NodeType::LIST == node1.type) {
    if (query_node::NodeType::LIST == node2.type) {
      node1.children.insert(
        node1.children.end(),
        node2.children.begin(),
        node2.children.end()
      );
    }
    else {
      node1.children.emplace_back(value2);
    }

    return value1; // ID of modified node
  }

  if (query_node::NodeType::LIST == node2.type) {
    return list(value2, value1);
  }

  parser::semantic_type value;
  auto& node = create_node(value);

  node.type = query_node::NodeType::LIST;
  node.children.emplace_back(value1);
  node.children.emplace_back(value2);

  return value; // ID of new node
}

////////////////////////////////////////////////////////////////////////////////
/// @brief negate the result of the specified node a value to a node
/// @return ID of the negated node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::negation(
  parser::semantic_type const& value
) {
  auto nodeId = try_eval(value, true, false, false); // only boolean
  auto nodeItr = m_negatedNodeCache.find(nodeId);

  if (nodeItr != m_negatedNodeCache.end()) {
    return nodeItr->second;
  }

  auto& node = find_node(nodeId);
  parser::semantic_type newValue;

  switch (node.type) {
  case query_node::NodeType::FUNCTION:
    if (!node.pFnBoolean) {
      return *const_cast<parser::semantic_type*>(&UNKNOWN); // only boolean
    }
    // fall through
  case query_node::NodeType::BOOL_TRUE: // fall through
  case query_node::NodeType::EQUAL: // fall through
  case query_node::NodeType::LIKE:
    {
      auto& newNode = create_node(newValue);

      newNode = node;
      newNode.bNegated = !newNode.bNegated;
    }

    break;
  case query_node::NodeType::INTERSECTION:
    {
      auto& newNode = create_node(newValue);

      newNode = node;
      newNode.type = query_node::NodeType::UNION;

      for (auto& child: newNode.children) {
        child = negation(child);

        if (child == UNKNOWN) {
          return *const_cast<parser::semantic_type*>(&UNKNOWN);
        }
      }
    }

    break;
  case query_node::NodeType::UNION:
    {
      if (node.children.empty()) {
        return *const_cast<parser::semantic_type*>(&UNKNOWN);
      }

      auto itr = node.children.begin();

      newValue = negation(*(itr++));

      for (auto end = node.children.end(); itr != end; ++itr) {
        newValue = op_and(newValue, negation(*itr));
      }
    }

    break;
  default:
    return *const_cast<parser::semantic_type*>(&UNKNOWN);
  }

  m_negatedNodeCache.emplace(nodeId, newValue);
  m_negatedNodeCache.emplace(newValue, nodeId);

  return newValue; // ID of new node
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct a range from the operands
/// @return ID of the range definition node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::range(
  parser::semantic_type const& value1, bool bInclusive1,
  parser::semantic_type const& value2, bool bInclusive2
) {
  auto minNodeId = try_eval(value1, false, false, true); // only sequence nodes
  auto maxNodeId = value1 == value2 ? minNodeId : try_eval(value2, false, false, true); // only sequence nodes (don't try_eval(...) twice)
  auto& minNode = find_node(minNodeId);
  auto& maxNode = find_node(maxNodeId);

  // only support values are range parameters
  if (!((query_node::NodeType::FUNCTION == minNode.type && minNode.pFnSequence) || query_node::NodeType::SEQUENCE == minNode.type || query_node::NodeType::UNKNOWN == minNode.type) ||
      !((query_node::NodeType::FUNCTION == maxNode.type && maxNode.pFnSequence) || query_node::NodeType::SEQUENCE == maxNode.type || query_node::NodeType::UNKNOWN == maxNode.type) ||
      (query_node::NodeType::UNKNOWN == minNode.type && query_node::NodeType::UNKNOWN == maxNode.type)) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN);
  }

  parser::semantic_type value;
  auto& node = create_node(value);

  node.type = query_node::NodeType::RANGE;
  node.bBeginInclusive = bInclusive1;
  node.bEndInclusive = bInclusive2;
  node.children.emplace_back(minNodeId);
  node.children.emplace_back(maxNodeId);

  return value;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             comparison operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief compare operands for equality
/// @return ID of the range definition node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::op_eq(
  parser::semantic_type const& value1, parser::semantic_type const& value2
) {
  auto nameNodeId = try_eval(value1, false, false, true); // accept only sequences
  auto& rangeNodeId = value2;
  auto& nameNode = find_node(nameNodeId);
  auto& rangeNode = find_node(rangeNodeId);

  // only support values are range parameters
  if (!((query_node::NodeType::FUNCTION == nameNode.type && nameNode.pFnSequence) || query_node::NodeType::SEQUENCE == nameNode.type) ||
      query_node::NodeType::RANGE != rangeNode.type) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN);
  }

  parser::semantic_type value;
  auto& node = create_node(value);

  node.type = query_node::NodeType::EQUAL;
  node.children.emplace_back(nameNodeId);
  node.children.emplace_back(rangeNodeId);

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare operands for likeness (i.e. phrase query)
/// @return ID of the range definition node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::op_like(
  parser::semantic_type const& value1, parser::semantic_type const& value2
) {
  auto leftNodeId = try_eval(value1, false, false, true); // only sequence
  auto rightNodeId = try_eval(value2, false, false, true); // only sequence
  auto& node1 = find_node(leftNodeId);
  auto& node2 = find_node(rightNodeId);

  // only support values are range parameters
  if (!((query_node::NodeType::FUNCTION == node1.type && node1.pFnSequence) || query_node::NodeType::SEQUENCE == node1.type) ||
      !((query_node::NodeType::FUNCTION == node2.type && node2.pFnSequence) || query_node::NodeType::SEQUENCE == node2.type)) {
    return *const_cast<parser::semantic_type*>(&UNKNOWN);
  }

  parser::semantic_type value;
  auto& node = create_node(value);

  node.type = query_node::NodeType::LIKE;
  node.children.emplace_back(leftNodeId);
  node.children.emplace_back(rightNodeId);

  return value;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 filter operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief logically AND two nodes
/// @return ID of the logically modified node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::op_and(
  parser::semantic_type const& value1, parser::semantic_type const& value2
) {
  auto leftNodeId = try_eval(value1, true, false, false); // only boolean
  auto rightNodeId = try_eval(value2, true, false, false); // only boolean
  auto& node1 = find_node(leftNodeId);
  auto& node2 = find_node(rightNodeId);

  switch (node1.type) {
  case query_node::NodeType::INTERSECTION:
    switch (node2.type) {
    case query_node::NodeType::INTERSECTION:
      {
        parser::semantic_type value = leftNodeId;

        for (auto& child: node2.children) {
          value = op_and(value, child);
        }

        return value; // ID of new node
      }
    case query_node::NodeType::UNION:
      return op_and(rightNodeId, leftNodeId);
    case query_node::NodeType::FUNCTION:
      if (!node2.pFnBoolean) {
        break; // can only have boolean functions in intersections
      }
      // fall through
    case query_node::NodeType::EQUAL: // fall through
    case query_node::NodeType::LIKE:
      add_child(node1.children, rightNodeId, false);

      return leftNodeId; // ID of modified node
    case query_node::NodeType::BOOL_TRUE:
      return node2.bNegated ? rightNodeId : leftNodeId;
    default: {} // NOOP
    }

    break;
  case query_node::NodeType::UNION:
    switch (node2.type) {
    case query_node::NodeType::UNION:
      {
        parser::semantic_type value;
        auto& node = create_node(value);

        node.type = query_node::NodeType::UNION;

        for (auto& child1: node1.children) {
          for (auto && child2: node2.children) {
            auto child = op_and(child1, child2);

            if (child == UNKNOWN) {
              return *const_cast<parser::semantic_type*>(&UNKNOWN);
            }

            add_child(node.children, child, true);
          }
        }

        return value;
      }
    case query_node::NodeType::FUNCTION:
      if (!node2.pFnBoolean) {
        break; // can only have boolean functions in intersections
      }
      // fall through
    case query_node::NodeType::INTERSECTION: // fall through
    case query_node::NodeType::EQUAL: // fall through
    case query_node::NodeType::LIKE:
      {
        parser::semantic_type value;
        auto& node = create_node(value);

        node.type = query_node::NodeType::UNION;

        for (auto& child1: node1.children) {
          parser::semantic_type value2Copy;
          auto& node2Copy = create_node(value2Copy);

          node2Copy = node2;

          //auto child = op_and(child1, value2Copy);
          auto child = op_and(child1, rightNodeId);

          if (child == UNKNOWN) {
            return *const_cast<parser::semantic_type*>(&UNKNOWN);
          }

          add_child(node.children, child, true);
        }

        return node.children.size() == 1 ? node.children[0] : value; // ID of existing/new node
      }
    case query_node::NodeType::BOOL_TRUE:
      return node2.bNegated ? rightNodeId : leftNodeId;
    default: {} // NOOP
    }

    break;
  case query_node::NodeType::FUNCTION:
    if (!node1.pFnBoolean) {
      break; // can only have boolean functions in intersections
    }
    // fall through
  case query_node::NodeType::EQUAL: // fall through
  case query_node::NodeType::LIKE:
    switch (node2.type) {
    case query_node::NodeType::INTERSECTION: // fall through
    case query_node::NodeType::UNION:
      return op_and(rightNodeId, leftNodeId);
    case query_node::NodeType::FUNCTION:
      if (!node2.pFnBoolean) {
        break; // can only have boolean functions in intersections
      }
      // fall through
    case query_node::NodeType::EQUAL: // fall through
    case query_node::NodeType::LIKE:
      if (leftNodeId == rightNodeId) {
        return leftNodeId; // ID of unmodified node
      }

      {
        parser::semantic_type value;
        auto& node = create_node(value);

        node.children.emplace_back(leftNodeId);
        node.children.emplace_back(rightNodeId);
        node.type = query_node::NodeType::INTERSECTION;

        return value; // ID of new node
      }
    case query_node::NodeType::BOOL_TRUE:
      return node2.bNegated ? rightNodeId : leftNodeId;
    default: {} // NOOP
    }

    break;
  case query_node::NodeType::BOOL_TRUE:
    return node1.bNegated ? leftNodeId : rightNodeId;
  default: {} // NOOP
  }

  return *const_cast<parser::semantic_type*>(&UNKNOWN);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logically OR two nodes
/// @return ID of the logically modified node or UNKNOWN on error
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::op_or(
  parser::semantic_type const& value1, parser::semantic_type const& value2
) {
  auto leftNodeId = try_eval(value1, true, false, false); // only boolean
  auto rightNodeId = try_eval(value2, true, false, false); // only boolean
  auto& node1 = find_node(leftNodeId);
  auto& node2 = find_node(rightNodeId);

  switch (node1.type) {
  case query_node::NodeType::UNION:
    switch (node2.type) {
    case query_node::NodeType::UNION:
      {
        parser::semantic_type value = leftNodeId;

        for (auto& child: node2.children) {
          value = op_or(value, child);
        }

        return value; // ID of new node
      }
    case query_node::NodeType::FUNCTION:
      if (!node2.pFnBoolean) {
        break; // can only have boolean functions in intersections
      }
      // fall through
    case query_node::NodeType::INTERSECTION: // fall through
    case query_node::NodeType::EQUAL: // fall through
    case query_node::NodeType::LIKE:
      add_child(node1.children, rightNodeId, true);

      return leftNodeId; // ID of modified node
    case query_node::NodeType::BOOL_TRUE:
      return node2.bNegated ? leftNodeId : rightNodeId;
    default: {} // NOOP
    }

    break;
  case query_node::NodeType::FUNCTION:
    if (!node1.pFnBoolean) {
      break; // can only have boolean functions in intersections
    }
    // fall through
  case query_node::NodeType::INTERSECTION: // fall through
  case query_node::NodeType::EQUAL: // fall through
  case query_node::NodeType::LIKE:
    switch (node2.type) {
    case query_node::NodeType::UNION:
      return op_or(rightNodeId, leftNodeId);
    case query_node::NodeType::FUNCTION:
      if (!node2.pFnBoolean) {
        break; // can only have boolean functions in intersections
      }
      // fall through
    case query_node::NodeType::INTERSECTION: // fall through
    case query_node::NodeType::EQUAL: // fall through
    case query_node::NodeType::LIKE:
      if (leftNodeId == rightNodeId) {
        return leftNodeId; // ID of unmodified node
      }

      {
        parser::semantic_type value;
        auto& node = create_node(value);

        node.children.emplace_back(leftNodeId);
        node.children.emplace_back(rightNodeId);
        node.type = query_node::NodeType::UNION;

        return value; // ID of new node
      }
    case query_node::NodeType::BOOL_TRUE:
      return node2.bNegated ? leftNodeId : rightNodeId;
    default: {} // NOOP
    }

    break;
  case query_node::NodeType::BOOL_TRUE:
    return node1.bNegated ? rightNodeId : leftNodeId;
  default: {} // NOOP
  }

  return *const_cast<parser::semantic_type*>(&UNKNOWN);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  query operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add an order field
/// @return success
////////////////////////////////////////////////////////////////////////////////
bool parser_context::addOrder(
  parser::semantic_type const& value, bool bAscending
) {
  auto nodeId = try_eval(value, false, true, true); // accept order or sequence
  auto& node = find_node(nodeId);

  switch (node.type) {
    case query_node::NodeType::FUNCTION: // fall through
      if (!node.pFnOrder && !node.pFnSequence) {
        break; // no applicable functions
      }
    [[fallthrough]];
    case query_node::NodeType::SEQUENCE:
      m_order.emplace_back(nodeId, bAscending);

      return true;
    default: {} // NOOP
  }

  return false; // no other types of nodes are supported
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a limit on the result set
/// @return success
////////////////////////////////////////////////////////////////////////////////
bool parser_context::setLimit(parser::semantic_type const& value) {
  auto nodeId = try_eval(value, false, false, true); // only sequence
  auto& node = find_node(nodeId);

  // only support values that are available during expression compile time
  if (query_node::NodeType::SEQUENCE != node.type) {
    return false; // no other types of nodes are supported
  }

  char const* pcStart = node.sValue.c_str();
  char* pcNext;
  float fValue = strtof(pcStart, &pcNext);

  m_limit.second = (size_t)fValue;
  m_limit.first =
    fValue == (float)(m_limit.second) &&
    (size_t)(pcNext - pcStart) == node.sValue.size();

  return m_limit.first;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the filter portion of a query
/// @return success
////////////////////////////////////////////////////////////////////////////////
bool parser_context::setQuery(parser::semantic_type const& value) {
  auto nodeId = try_eval(value, true, false, false); // only accept boolean
  auto& node = find_node(nodeId);

  // only support values that are conditional expressions
  switch (node.type) {
    case query_node::NodeType::FUNCTION:
      if (!node.pFnBoolean) {
        break; // only boolean functions allowed as query root
      }
    [[fallthrough]];
    case query_node::NodeType::UNION: // fall through
    case query_node::NodeType::INTERSECTION: // fall through
    case query_node::NodeType::BOOL_TRUE: // fall through
    case query_node::NodeType::EQUAL: // fall through
    case query_node::NodeType::LIKE:
      m_filter.first = true;
      m_filter.second = nodeId;

      return true;
    default: {} // NOOP
  }

  m_filter.first = false;

  return false; // no other types of nodes are supported
}

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieve current state of the context
///        'pLastError' is set to values from last call to yyerror(...)
/// @return the next position to be parsed
////////////////////////////////////////////////////////////////////////////////
parser_context::query_state parser_context::current_state() const {
  return {
    /*nOffset =*/  m_nNext,
    /*pnFilter =*/ m_filter.first ? &m_filter.second : nullptr,
    /*order =*/    m_order,
    /*pnLimit =*/  m_limit.first ? &m_limit.second : nullptr,
    /*pError =*/   m_error.first ? &m_error.second : nullptr
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieve a node by 'value'
/// @return requested node or UNKNOWN if not found
////////////////////////////////////////////////////////////////////////////////
parser_context::query_node const& parser_context::find_node(
  parser::semantic_type const& value
) const {
  #if defined(__APPLE__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wtautological-compare"
  #elif defined (__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wtype-limits"
  #elif defined(_MSC_VER)
    #pragma warning(disable: 4127) // conditional expression is constant
  #endif

    // parser::semantic_type may be defined as a signed value in parser.yy
    if (std::is_signed<parser::semantic_type>::value && value < 0) {
      return m_nodes[0];
    }

  #if defined(__APPLE__)
    #pragma clang diagnostic pop
  #elif defined (__GNUC__)
    #pragma GCC diagnostic pop
  #elif defined(_MSC_VER)
    #pragma warning(default: 4127)
  #endif

  return value < m_nodes.size() ? m_nodes[value] : m_nodes[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief output the branch starting at 'root' as a string representation
////////////////////////////////////////////////////////////////////////////////
void parser_context::print(
  std::ostream & out, parser::semantic_type const& root, bool bBoost, bool bId
) {
  auto const& node = find_node(root);
  std::string sChildDelim = "";

  switch (node.type) {
  case query_node::NodeType::UNION:
    if (bBoost) out << node.fBoost << "*";
    out << "{";
    sChildDelim = " || ";
    break;
  case query_node::NodeType::INTERSECTION:
    if (bBoost) out << node.fBoost << "*";
    out << "{";
    sChildDelim = " && ";
    break;
  case query_node::NodeType::EQUAL:
    if (bBoost) out << node.fBoost << "*";
    if (bBoost || bId) out << "(";
    sChildDelim = node.bNegated ? " != " : " == ";
    break;
  case query_node::NodeType::LIKE:
    if (bBoost) out << node.fBoost << "*";
    if (bBoost || bId) out << node.fBoost << "(";
    sChildDelim = node.bNegated ? " !~= " : " ~= ";
    break;
  case query_node::NodeType::FUNCTION: // fall through
    out << "'" << node.sValue << "'(";
    sChildDelim = ", ";
    break;
  case query_node::NodeType::LIST:
    out << "(";
    break;
  case query_node::NodeType::RANGE:
    if (node.children.size() == 2 && node.children[0] == node.children[1]) {
      print(out, node.children[0], bBoost, bId); // same node, i.e. ==
      return;
    }
    out << (node.bBeginInclusive ? "[" : "(");
    sChildDelim = ", ";
    break;
  case query_node::NodeType::SEQUENCE:
    out << "'" << node.sValue << "'";
    if (bId) out << "@" << root;
    return;
  default:
    out << "\?\?\?(" << node.type << ")";
    if (bId) out << "@" << root;
    return;
  }

  std::string sDelim = "";

  for (auto& child: node.children) {
    out << sDelim;
    print(out, child, bBoost, bId);
    sDelim = sChildDelim;
  }

  switch (node.type) {
  case query_node::NodeType::UNION: // fall through
  case query_node::NodeType::INTERSECTION: // fall through
    out << "}";
    if (bId) out << "@" << root;
    return;
  case query_node::NodeType::EQUAL: // fall through
  case query_node::NodeType::LIKE:
    if (bBoost || bId) out << ")";
    if (bId) out << "@" << root;
    return;
  case query_node::NodeType::FUNCTION: // fall through
  case query_node::NodeType::LIST:
    out << ")";
    if (bId) out << "@" << root;
    return;
  case query_node::NodeType::RANGE:
    out << (node.bEndInclusive ? "]" : ")");
    if (bId) out << "@" << root;
    return;
  default: {} // NOOP
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add a child to children, ensuring there are no duplicates or overlaps
////////////////////////////////////////////////////////////////////////////////
void parser_context::add_child(
    std::vector<size_t>& children,
    parser::semantic_type const& child,
    bool bRemoveSuperset) {
  auto& node = find_node(child);
  absl::flat_hash_set<size_t> subChildren; // only for bRemoveSuperset

  if (bRemoveSuperset) {
    if (query_node::NodeType::INTERSECTION == node.type) {
      subChildren.insert(node.children.begin(), node.children.end());
    }
    else {
      subChildren.emplace(child);
    }
  }

  for (auto itr = children.begin(); itr != children.end();) {
    auto& existing = *(itr++);

    if (existing == child) {
      return; // nothing to do, child already present
    }

    if (!bRemoveSuperset) {
      continue;
    }

    // ...........................................................................
    // check if one of the sub-children is an intersection superset
    // e.g. (A && B) || (A && B && C) <-- pick (A && B)
    // ...........................................................................
    auto& existingNode = find_node(existing);

    if (query_node::NodeType::INTERSECTION != existingNode.type) {
      continue;
    }

    size_t nMatching = 0;

    // count number of children from existingNode that exist in child.children
    for (auto& existingSubChild : existingNode.children) {
      if (subChildren.find(existingSubChild) != subChildren.end()) {
        ++nMatching;
      }
    }

    // all existingNode.children in child.children
    // e.g. existing:(A && B), new:(A && B && C) <-- pick existing
    if (existingNode.children.size() == nMatching) {
      return;
    }

    // all child.children in existingNode.children
    // e.g. existing:(A && B && C), new:(A && B) <-- pick new
    if (subChildren.size() == nMatching) {
      itr = children.erase(itr - 1);
    }
  }

  children.emplace_back(child);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new node
///        NOTE: previous results from find_node(...) are undefined after return
/// @return new node and set 'value' to ID of new node
////////////////////////////////////////////////////////////////////////////////
parser_context::query_node& parser_context::create_node(
  parser::semantic_type& value
) {
  m_nodes.emplace_back();
  value = m_nodes.size() - 1;

  return m_nodes.back();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a function in 'fns' best matching the supplied arguments
/// @return pointer to function or nullptr if no best match found
////////////////////////////////////////////////////////////////////////////////
template <typename function_type>
function_type const* iresearch::iql::parser_context::find_best_function(
  std::string const& sName,
  size_t nArgsCount,
  std::unordered_multimap<std::string, function_type> const& fns
) const {
  auto fnItr = fns.equal_range(sName);
  function_type const* pBestFn = nullptr; // init to non-match value

  // find best matching function based on number of arguments
  for (auto itr = fnItr.first; itr != fnItr.second; ++itr) {
    auto& fn = itr->second;

    if (fn.m_nFixedArg > nArgsCount) {
      continue; // too many arguments
    }

    if (fn.m_nFixedArg == nArgsCount || pBestFn == nullptr) {
      if (pBestFn != nullptr && pBestFn->m_nFixedArg == fn.m_nFixedArg) {
        return nullptr; // collision
      }

      pBestFn = &fn;
    }
    else if (fn.m_bVarArg) {
      if (pBestFn->m_nFixedArg == fn.m_nFixedArg) {
        return nullptr; // collision
      }

      // fn is a better match since it has more fixed args
      if (pBestFn->m_nFixedArg < fn.m_nFixedArg) {
        pBestFn = &fn;
      }
    }
  }

  return pBestFn;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieve a node by 'value'
/// @return requested node or UNKNOWN if not found
////////////////////////////////////////////////////////////////////////////////
parser_context::query_node& parser_context::find_node(
  parser::semantic_type const& value
) {
  // reuse const-implementation
  auto& node = const_cast<const parser_context*>(this)->find_node(value);

  return const_cast<parser_context::query_node&>(node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read next token
/// @return token type
////////////////////////////////////////////////////////////////////////////////
parser::token_type parser_context::next() {
  if (m_nNext >= m_sData.size()) {
    return parser::token_type::IQL_EOF;
  }

  switch (m_eState) {
  case StateType::SINGLE: {
    parser::token_type type = nextQuoted(false); // check for sequence end

    return type != parser::token_type::IQL_UNKNOWN ? type : nextSequence('\'');
  }
  case StateType::DOUBLE: {
    parser::token_type type = nextQuoted(false); // check for sequence end

    return type != parser::token_type::IQL_UNKNOWN ? type : nextSequence('"');
  }
  case StateType::NONE:
    break;
  default:
    return parser::token_type::IQL_UNKNOWN; // unsupported state
  }

  // ...........................................................................
  // check if it's whitespace
  // ...........................................................................
  parser::token_type type;
  if ((type = nextSeperator()) != parser::token_type::IQL_UNKNOWN) {
    return type;
  }

  // ...........................................................................
  // check if it's a quoted literal
  // ...........................................................................
  if ((type = nextQuoted(true)) != parser::token_type::IQL_UNKNOWN) {
    return type;
  }

  // ...........................................................................
  // check if it's an operator
  // ...........................................................................
  if ((type = nextOperator()) != parser::token_type::IQL_UNKNOWN) {
    return type;
  }

  // ...........................................................................
  // check if it's a keyword
  // ...........................................................................
  if ((type = nextKeyword()) != parser::token_type::IQL_UNKNOWN) {
    return type;
  }

  bool bSeen = false;

  while (m_nNext < m_sData.size() &&
         !isspace((uint8_t)(m_sData[m_nNext])) &&
         (!bSeen || !ispunct((uint8_t)(m_sData[m_nNext])))) { // allow 1 char ispunct(...)
    ++m_nNext;
    bSeen = true;
  }

  return bSeen ?
    parser::token_type::IQL_SEQUENCE : parser::token_type::IQL_UNKNOWN;
}

constexpr frozen::unordered_map<frozen::string, parser::token_type, 7> KEYWORDS = {
  { "NOT",   parser::token_type::IQL_NOT },
  { "AND",   parser::token_type::IQL_AND },
  { "OR",    parser::token_type::IQL_OR },
  { "ORDER", parser::token_type::IQL_ORDER },
  { "ASC",   parser::token_type::IQL_ASC },
  { "DESC",  parser::token_type::IQL_DESC },
  { "LIMIT", parser::token_type::IQL_LIMIT },
};

////////////////////////////////////////////////////////////////////////////////
/// @brief read next token as a keyword
///        do not modify m_nNext if IQL_UNKNOWN
/// @return token type or IQL_UNKNOWN if not a keyword
////////////////////////////////////////////////////////////////////////////////
parser::token_type parser_context::nextKeyword() {
  size_t nEnd = m_nNext;

  // find end of token
  for (size_t nCount = m_sData.size(); nEnd < nCount; ++nEnd) {
    if (isspace((uint8_t)(m_sData[nEnd])) ||
        ispunct((uint8_t)(m_sData[nEnd]))) {
      break;
    }
  }

  std::string sValue = m_sData.substr(m_nNext, nEnd - m_nNext); // ci value

  std::transform(sValue.begin(), sValue.end(), sValue.begin(), ::toupper);

  const auto itr = KEYWORDS.find(frozen::string{sValue.c_str(), sValue.size()});

  if (itr == KEYWORDS.end()) {
    return parser::token_type::IQL_UNKNOWN;
  }

  m_nNext = nEnd;

  return itr->second;
}


// ...........................................................................
// single char operators
// ...........................................................................
constexpr frozen::unordered_map<char, parser::token_type, 9> OPERATORS1C = {
  { ',', parser::token_type::IQL_COMMA },
  { '*', parser::token_type::IQL_ASTERISK },
  { '<', parser::token_type::IQL_LCHEVRON },
  { '>', parser::token_type::IQL_RCHEVRON },
  { '!', parser::token_type::IQL_EXCLAIM },
  { '(', parser::token_type::IQL_LPAREN },
  { ')', parser::token_type::IQL_RPAREN },
  { '[', parser::token_type::IQL_LSBRACKET },
  { ']', parser::token_type::IQL_RSBRACKET },
};

////////////////////////////////////////////////////////////////////////////////
/// @brief read next token as an operator
///        do not modify m_nNext if IQL_UNKNOWN
/// @return token type or IQL_UNKNOWN if not an operator
////////////////////////////////////////////////////////////////////////////////
parser::token_type parser_context::nextOperator() {
  // ...........................................................................
  // double char operators
  // ...........................................................................
  if (m_nNext + 1 < m_sData.size()) {
    char const* pcStart = &(m_sData.c_str()[m_nNext]);
    parser::token_type type = parser::token_type::IQL_UNKNOWN;

    if (strncmp("~=", pcStart, 2) == 0) {
      type = parser::token_type::IQL_LIKE;
    }
    else if (strncmp("!=", pcStart, 2) == 0 ||
             strncmp("<>", pcStart, 2) == 0) {
      type = parser::token_type::IQL_NE;
    }
    else if (strncmp("<=", pcStart, 2) == 0) {
      type = parser::token_type::IQL_LE;
    }
    else if (strncmp("==", pcStart, 2) == 0) {
      type = parser::token_type::IQL_EQ;
    }
    else if (strncmp(">=", pcStart, 2) == 0) {
      type = parser::token_type::IQL_GE;
    }
    else if (strncmp("&&", pcStart, 2) == 0) {
      type = parser::token_type::IQL_AMPAMP;
    }
    else if (strncmp("||", pcStart, 2) == 0) {
      type = parser::token_type::IQL_PIPEPIPE;
    }

    if (parser::token_type::IQL_UNKNOWN != type) {
      m_nNext += 2;

      return type;
    }
  }

  const auto itr = OPERATORS1C.find(m_sData[m_nNext]);

  if (itr != OPERATORS1C.end()) {
    ++m_nNext;

    return itr->second;
  }

  return parser::token_type::IQL_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read next token as a start of a quoted sequence
///        do not modify m_nNext if IQL_UNKNOWN
/// @return token type or IQL_UNKNOWN if not an operator
////////////////////////////////////////////////////////////////////////////////
parser::token_type parser_context::nextQuoted(bool bStart) {
  switch (m_sData[m_nNext]) {
  case '"':
    if (!bStart && StateType::DOUBLE != m_eState) {
      break; // inside an existing sequence that is not double-quoted
    }

    ++m_nNext;
    m_eState = bStart ? StateType::DOUBLE : StateType::NONE;

    return parser::token_type::IQL_DQUOTE;
  case '\'':
    if (!bStart && StateType::SINGLE != m_eState) {
      break; // inside an existing sequence that is not single-quoted
    }

    ++m_nNext;
    m_eState = bStart ? StateType::SINGLE : StateType::NONE;

    return parser::token_type::IQL_SQUOTE;
  }

  return parser::token_type::IQL_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read next seperator sequence
/// @return token type
////////////////////////////////////////////////////////////////////////////////
parser::token_type parser_context::nextSeperator() {
  size_t nCount = m_sData.size();
  bool bSeen = false;

  // skip to the end of whitespace
  while (m_nNext < nCount && isspace((uint8_t)(m_sData[m_nNext]))) {
    ++m_nNext;
    bSeen = true;
  }

  // treat comments as part of whitespace, i.e. /* ... */
  if (m_nNext < nCount + 2 &&
      strncmp("/*", &(m_sData.c_str()[m_nNext]), 2) == 0) {
    for (size_t i = m_nNext + 2; i + 1 < nCount; ++i) { // +2 for /*, +1 for /
      // if found comment terminator
      if (strncmp("*/", &(m_sData.c_str()[i]), 2) == 0) {
        m_nNext = i + 2; // +2 for */
        nextSeperator(); // consume any subsequence spaces and comments

        return  parser::token_type::IQL_SEP;
      }
    }
    // not a comment since no */ found
  }

  return bSeen ?
    parser::token_type::IQL_SEP : parser::token_type::IQL_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read next sequence terminated by character 'cSep'
///        do not modify m_nNext if IQL_UNKNOWN
/// @return token type or IQL_UNKNOWN if not a terminated sequence
////////////////////////////////////////////////////////////////////////////////
parser::token_type parser_context::nextSequence(char cSep) {
  for (size_t i = m_nNext, nCount = m_sData.size(); i < nCount; ++i) {
    if (cSep == m_sData[i]) {
      m_nNext = i;

      return parser::token_type::IQL_SEQUENCE;
    }
  }

  return parser::token_type::IQL_UNKNOWN;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief evaluate a node (e.g. a deterministic function node) if possible
/// @return ID of the node with the evaluated data or the original node if no
///         further valuation can be applied, UNKNOWN on name collision
////////////////////////////////////////////////////////////////////////////////
parser::semantic_type parser_context::try_eval(
  parser::semantic_type value, bool bBoolean, bool bOrder, bool bSequence
) {
    auto& node = find_node(value);

    if (query_node::NodeType::FUNCTION != node.type) {
      return value; // cannot eval
    }

    bBoolean &= node.pFnBoolean != nullptr;
    bOrder &= node.pFnOrder != nullptr;
    bSequence &= node.pFnSequence != nullptr;

    if ((bBoolean && (bOrder || bSequence)) || (bOrder && bSequence)) {
      return *const_cast<parser::semantic_type*>(&UNKNOWN); // name collision
    }

    if (!bBoolean && !bOrder && !bSequence) {
      return value; // cannot eval
    }

    // ...........................................................................
    // auto-evaluate determinitic values during parsing if possible
    // ...........................................................................

    std::vector<iresearch::string_ref> fnArgs;
    bool bDeterministic = true;

    // check if one of the args is non-deterministic
    for (auto& child: node.children) {
      child = try_eval(child, false, false, true); // only sequence nodes

      auto& childNode = find_node(child);

      // only SEQUENCE args are supported by determinitic functions
      if (query_node::NodeType::SEQUENCE != childNode.type) {
        bDeterministic = false; // not a deterministic argument, cannot eval
      }

      fnArgs.emplace_back(childNode.sValue);
    }

    if (!bDeterministic) {
      return value; // not all arguments deterministic, cannot eval
    }

    if (bBoolean) {
      bool bResult;

      // check for successful invocation of the deterministic function
      if (!node.pFnBoolean->m_fnDeterminitic(bResult, fnArgs)) {
        return value; // not a deterministic function, cannot eval
      }

        return bResult ? TRUE : negation(TRUE); // TRUE/FALSE eval
    }

    if (bOrder) {
      order_function::deterministic_buffer_t buf;

      // check for successful invocation of the deterministic function
      if (!node.pFnOrder->m_fnDeterminitic(buf, fnArgs)) {
        return value; // not a deterministic function, cannot eval
      }

      auto& order_node = create_node(value);

      order_node.sValue = buf;
      order_node.type = query_node::NodeType::SEQUENCE;

      return value; // ID of new node
    }

    if (bSequence) {
      sequence_function::deterministic_buffer_t buf;

      // check for successful invocation of the deterministic function
      if (!node.pFnSequence->m_fnDeterminitic(buf, fnArgs)) {
        return value; // not a deterministic function, cannot eval
      }

      auto& seq_node = create_node(value);

      seq_node.sValue = buf;
      seq_node.type = query_node::NodeType::SEQUENCE;

      return value; // ID of new node
    }

    return value; // deterministic valuation not possible
}
