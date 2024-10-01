////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/SearchFunc.h"

#include "VocBase/voc-types.h"

#include "search/scorer.hpp"

namespace arangodb {
namespace aql {

class Ast;
struct AstNode;
class CalculationNode;
class Expression;
struct Variable;

}  // namespace aql

namespace iresearch {

class IResearchViewNode;
struct QueryContext;

}  // namespace iresearch

namespace transaction {

class Methods;

}  // namespace transaction

namespace iresearch::order_factory {

// Extract reference from scorer node
aql::Variable const* refFromScorer(aql::AstNode const& node);

// Determine if the 'node' can be converted into an iresearch scorer
// if 'scorer' != nullptr then also append build iresearch scorer
// there.
bool scorer(irs::Scorer::ptr* scorer, aql::AstNode const& node,
            QueryContext const& ctx);

// Determine if the 'node' can be converted into an iresearch scorer
// if 'scorer' != nullptr then also append build iresearch comparer
// there
bool comparer(irs::Scorer::ptr* scorer, aql::AstNode const& node);

}  // namespace iresearch::order_factory
}  // namespace arangodb
