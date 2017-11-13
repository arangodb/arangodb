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

#include "search/filter.hpp"

NS_BEGIN(iresearch)


NS_END // iresearch

NS_BEGIN(arangodb)
NS_BEGIN(aql)

class SortCondition; // forward declaration

NS_END // aql

NS_BEGIN(transaction)

class Methods; // forward declaration

NS_END // transaction

NS_BEGIN(iresearch)

struct IResearchViewMeta; // forward declaration

struct OrderFactory {
  struct OrderContext {
    std::vector<irs::stored_attribute::ptr>& attributes;
    irs::order& order;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch order
  ///        if 'order' != nullptr then also append the iresearch order there
  ////////////////////////////////////////////////////////////////////////////////
  static bool order(
    OrderContext* ctx,
    arangodb::aql::SortCondition const& node,
    IResearchViewMeta const& meta
  );
}; // OrderFactory

NS_END // iresearch
NS_END // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_ORDER_FACTORY_H
