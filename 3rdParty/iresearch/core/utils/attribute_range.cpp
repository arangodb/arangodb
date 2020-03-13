////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "attribute_range.hpp"
#include "search/disjunction.hpp"

NS_ROOT

template<>
bool attribute_range<position_score_iterator_adapter<doc_iterator::ptr>>::next() {
  assert(ars_);
  value_ = ars_->get_next_iterator();
  return value_ != nullptr;
}

#define DEFINE_ATTRIBUTE_RANGE_TYPE(AttributeRange) template<> IRESEARCH_API \
/*static*/ const attribute::type_id& AttributeRange::type() { \
  static attribute::type_id type(#AttributeRange); \
  return type; \
}

#define DEFINE_ATTRIBUTE_RANGE(Adapter) DEFINE_ATTRIBUTE_RANGE_TYPE(attribute_range<Adapter>);

DEFINE_ATTRIBUTE_RANGE(score_iterator_adapter<doc_iterator::ptr>);
DEFINE_ATTRIBUTE_RANGE(position_score_iterator_adapter<doc_iterator::ptr>);

NS_END // ROOT
