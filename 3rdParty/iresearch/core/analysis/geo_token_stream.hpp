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

#ifndef IRESEARCH_GEO_TOKEN_STREAM_H
#define IRESEARCH_GEO_TOKEN_STREAM_H

#include <s2/s2region_term_indexer.h>

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_stream.hpp"
#include "utils/frozen_attributes.hpp"

NS_ROOT
NS_BEGIN(analysis)

class geo_token_stream final
  : public frozen_attributes<3, token_stream>,
    private util::noncopyable {
 public:
  static constexpr string_ref type_name() noexcept { return "geo"; }

  static void init(); // for triggering registration in a static build

  explicit geo_token_stream(const S2RegionTermIndexer::Options& opts,
                            const string_ref& prefix);

  virtual bool next() noexcept override;

  void reset(const S2Point& point);
  void reset(const S2Region& region);

 private:
  S2RegionTermIndexer indexer_;
  std::vector<std::string> terms_;
  const std::string* begin_{ terms_.data() };
  const std::string* end_{ begin_ };
  std::string prefix_;
  offset offset_;
  increment inc_;
  term_attribute term_;
}; // geo_token_stream

NS_END // analysis
NS_END // ROOT

#endif // IRESEARCH_GEO_TOKEN_STREAM_H

