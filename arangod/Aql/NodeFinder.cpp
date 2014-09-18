////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, NodeFinder
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

#include "Aql/NodeFinder.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief node finder for one node type
////////////////////////////////////////////////////////////////////////////////

template<>
NodeFinder<ExecutionNode::NodeType>::NodeFinder (ExecutionNode::NodeType lookingFor,
                                                 std::vector<ExecutionNode*>& out,
                                                 bool enterSubqueries) 
 : _lookingFor(lookingFor), 
   _out(out), 
   _enterSubqueries(enterSubqueries) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief node finder for multiple types
////////////////////////////////////////////////////////////////////////////////
    
template<>
NodeFinder<std::vector<ExecutionNode::NodeType>>::NodeFinder (std::vector<ExecutionNode::NodeType> lookingFor,
                                                              std::vector<ExecutionNode*>& out,
                                                              bool enterSubqueries) 
  : _lookingFor(lookingFor), 
    _out(out), 
    _enterSubqueries(enterSubqueries) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief before method for one node type
////////////////////////////////////////////////////////////////////////////////

template<>
bool NodeFinder<ExecutionNode::NodeType>::before (ExecutionNode* en) {
  if (en->getType() == _lookingFor) {
    _out.push_back(en);
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief before method for multiple node types
////////////////////////////////////////////////////////////////////////////////
    
template<>
bool NodeFinder<std::vector<ExecutionNode::NodeType>>::before (ExecutionNode* en) {
  auto const nodeType = en->getType();
  for (auto& type : _lookingFor) {
    if (type == nodeType) {
      _out.push_back(en);
      break; 
    }
  }
  return false;
}

  }  // namespace triagens::aql
}  // namespace triagens

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
