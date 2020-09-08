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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_GEO_ANALYZER
#define ARANGODB_IRESEARCH__IRESEARCH_GEO_ANALYZER 1

#include <s2/s2region_term_indexer.h>

#include "shared.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/analyzer.hpp"
#include "utils/frozen_attributes.hpp"

namespace arangodb {
namespace iresearch {

class GeoAnalyzer final
  : public irs::frozen_attributes<3, irs::analysis::analyzer>,
    private irs::util::noncopyable {
 public:
  struct Options {
    int32_t maxCells;
    int32_t minLevel;
    int32_t maxLevel;
    bool pointsOnly;
    bool optimizeForSpace;
  };

  static constexpr irs::string_ref type_name() noexcept { return "geo"; }
  static bool normalize(const irs::string_ref& args,  std::string& out);
  static irs::analysis::analyzer::ptr make(irs::string_ref const& args);

  GeoAnalyzer(const Options& opts, const irs::string_ref& prefix);

  virtual bool next() noexcept final;

  virtual bool reset(irs::string_ref const& value);
  virtual bool reset_for_querying(irs::string_ref const& value);

 private:
  S2RegionTermIndexer _indexer;
  std::vector<std::string> _terms;
  const std::string* _begin{ _terms.data() };
  const std::string* _end{ _begin };
  std::string _prefix;
  irs::offset _offset;
  irs::increment _inc;
  irs::term_attribute _term;
}; // GeoAnalyzer

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_GEO_ANALYZER
