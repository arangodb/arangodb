////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H
#define ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H 1

#include "VocBase/voc-types.h"

#include "search/sort.hpp"

NS_BEGIN(arangodb)
NS_BEGIN(aql)

struct AstNode;
struct Variable;

NS_END // aql

NS_BEGIN(iresearch)

struct QueryContext;

NS_END // iresearch

NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction

NS_BEGIN(iresearch)

struct OrderFactory {
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch scorer
  ///        if 'scorer' != nullptr then also append build iresearch scorer there
  ////////////////////////////////////////////////////////////////////////////////
  static bool scorer(
    irs::sort::ptr* scorer,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::QueryContext const& ctx
  );

  static bool comparer(
    irs::sort::ptr* scorer,
    arangodb::aql::AstNode const& node
  );
}; // OrderFactory

NS_END // iresearch
NS_END // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H
