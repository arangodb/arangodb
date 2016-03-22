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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Index.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"

using namespace arangodb::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the index
////////////////////////////////////////////////////////////////////////////////

Index::~Index() {
  if (ownsInternals) {
    TRI_ASSERT(internals != nullptr);
    delete internals;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a VelocyPack representation of the index
////////////////////////////////////////////////////////////////////////////////

void Index::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);

  builder.add("type", VPackValue(arangodb::Index::typeName(type)));
  builder.add("id", VPackValue(arangodb::basics::StringUtils::itoa(id)));
  builder.add("unique", VPackValue(unique));
  builder.add("sparse", VPackValue(sparse));

  if (hasSelectivityEstimate()) {
    builder.add("selectivityEstimate", VPackValue(selectivityEstimate()));
  }

  builder.add(VPackValue("fields"));
  {
    VPackArrayBuilder arrayGuard(&builder);
    for (auto const& field : fields) {
      std::string tmp;
      TRI_AttributeNamesToString(field, tmp);
      builder.add(VPackValue(tmp));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the index internals
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* Index::getInternals() const {
  if (internals == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "accessing undefined index internals");
  }
  return internals;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the index internals
////////////////////////////////////////////////////////////////////////////////

void Index::setInternals(arangodb::Index* idx, bool owns) {
  TRI_ASSERT(internals == nullptr);
  internals = idx;
  ownsInternals = owns;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether or not the index supports the filter condition
/// and calculate the filter costs and number of items
////////////////////////////////////////////////////////////////////////////////

bool Index::supportsFilterCondition(arangodb::aql::AstNode const* node,
                                    arangodb::aql::Variable const* reference,
                                    size_t itemsInIndex, size_t& estimatedItems,
                                    double& estimatedCost) const {
  if (!hasInternals()) {
    return false;
  }
  return getInternals()->supportsFilterCondition(node, reference, itemsInIndex,
                                                 estimatedItems, estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether or not the index supports the sort condition
/// and calculate the sort costs
////////////////////////////////////////////////////////////////////////////////

bool Index::supportsSortCondition(
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    double& estimatedCost) const {
  if (!hasInternals()) {
    return false;
  }
  return getInternals()->supportsSortCondition(sortCondition, reference,
                                               itemsInIndex, estimatedCost);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream,
                         arangodb::aql::Index const* index) {
  stream << index->getInternals()->context();
  return stream;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the index to an output stream
////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& stream,
                         arangodb::aql::Index const& index) {
  stream << index.getInternals()->context();
  return stream;
}
