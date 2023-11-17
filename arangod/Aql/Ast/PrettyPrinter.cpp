////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////
#include "PrettyPrinter.h"

#include <Aql/AstNode.h>
#include <Aql/Quantifier.h>
#include <Aql/Variable.h>
#include <velocypack/Builder.h>

#include <fmt/core.h>

using namespace arangodb::aql;

namespace arangodb::aql::ast::pretty_printer {

auto toStream(std::ostream& os, AstNode const* node, int level)
    -> std::ostream& {
  for (int i = 0; i < level; ++i) {
    os << "  ";
  }
  os << fmt::format("- {} ({})", node->getTypeString(), node->type);

  switch (node->type) {
    case NODE_TYPE_VALUE:
    case NODE_TYPE_ARRAY: {
      VPackBuilder b;
      node->toVelocyPackValue(b);
      os << ": " << b.toJson();
    } break;
    case NODE_TYPE_ATTRIBUTE_ACCESS: {
      os << ": " << node->getString();
    } break;
    case NODE_TYPE_REFERENCE: {
      os << ": " << static_cast<Variable const*>(node->getData())->name;
    } break;
    case NODE_TYPE_QUANTIFIER: {
      os << ": "
         << Quantifier::stringify(
                static_cast<Quantifier::Type>(node->getIntValue(true)));
    } break;
    default: {
      os << ": ";
    } break;
  }
  os << "\n";

  size_t const n = node->numMembers();

  for (size_t i = 0; i < n; ++i) {
    auto sub = node->getMemberUnchecked(i);
    toStream(os, sub, level + 1);
  }
  return os;
}

}  // namespace arangodb::aql::ast::pretty_printer
