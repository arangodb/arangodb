////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <variant>

#include "Inspection/Types.h"

#include "Cache/CacheOptionsProvider.h"

namespace arangodb::sepp {

struct CacheOptions : arangodb::CacheOptionsProvider {
  CacheOptions();

  template<class Inspector>
  inline friend auto inspect(Inspector& f, CacheOptions& o) {
    return f.object(o).fields(
        f.field("idealLowerFillRatio", o._options.idealLowerFillRatio)
            .fallback(f.keep()),
        f.field("idealUpperFillRatio", o._options.idealUpperFillRatio)
            .fallback(f.keep()),
        f.field("minValueSizeForEdgeCompression",
                o._options.minValueSizeForEdgeCompression)
            .fallback(f.keep()),
        f.field("accelerationFactorForEdgeCompression",
                o._options.accelerationFactorForEdgeCompression)
            .fallback(f.keep()),
        f.field("cacheSize", o._options.cacheSize).fallback(f.keep()),
        f.field("rebalancingInterval", o._options.rebalancingInterval)
            .fallback(f.keep()),
        f.field("maxSpareAllocation", o._options.maxSpareAllocation)
            .fallback(f.keep()));
  }

  arangodb::CacheOptions getOptions() const override { return _options; }

 private:
  arangodb::CacheOptions _options;
};

}  // namespace arangodb::sepp
