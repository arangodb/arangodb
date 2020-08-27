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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "geo_token_stream.hpp"

NS_ROOT
NS_BEGIN(analysis)

geo_token_stream::geo_token_stream(const S2RegionTermIndexer::Options& opts,
                                   const string_ref& prefix)
  : attributes{{
      { irs::type<increment>::id(), &inc_       },
      { irs::type<offset>::id(), &offset_       },
      { irs::type<term_attribute>::id(), &term_ }
    }},
    indexer_(opts),
    prefix_(prefix) {
}

bool geo_token_stream::next() noexcept {
  if (begin_ >= end_) {
    return false;
  }

  auto& value = *begin_++;

  term_.value = bytes_ref(
    reinterpret_cast<const byte_type*>(value.c_str()),
    value.size());

  return true;
}

void geo_token_stream::reset(const S2Point& point) {
  terms_ = indexer_.GetIndexTerms(point, prefix_);
  begin_ = terms_.data();
  end_ = begin_ + terms_.size();
}

void geo_token_stream::reset(const S2Region& region) {
  terms_ = indexer_.GetIndexTerms(region, prefix_);
  begin_ = terms_.data();
  end_ = begin_ + terms_.size();
}

NS_END // analysis
NS_END // ROOT
