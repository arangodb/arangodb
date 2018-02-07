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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_FILTER_FACTORY_H
#define ARANGOD_IRESEARCH__IRESEARCH_FILTER_FACTORY_H 1

#include "VocBase/voc-types.h"

#include "search/filter.hpp"

NS_BEGIN(iresearch)

class boolean_filter; // forward declaration

NS_END // iresearch

NS_BEGIN(arangodb)
NS_BEGIN(aql)

struct AstNode; // forward declaration

NS_END // aql

NS_BEGIN(iresearch)

struct QueryContext;

struct FilterFactory {
  static irs::filter::ptr filter(TRI_voc_cid_t cid);
  static irs::filter::ptr filter(TRI_voc_cid_t cid, TRI_voc_rid_t rid);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create a filter matching 'cid' + 'rid' pair and append to 'buf'
  /// @return the appended filter portion
  ////////////////////////////////////////////////////////////////////////////////
  static irs::filter& filter(
    irs::boolean_filter& buf,
    TRI_voc_cid_t cid,
    TRI_voc_rid_t rid
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief determine if the 'node' can be converted into an iresearch filter
  ///        if 'filter' != nullptr then also append the iresearch filter there
  ////////////////////////////////////////////////////////////////////////////////
  static bool filter(
    irs::boolean_filter* filter,
    QueryContext const& ctx,
    arangodb::aql::AstNode const& node
  );
}; // FilterFactory

NS_END // iresearch
NS_END // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_FILTER_FACTORY_H
