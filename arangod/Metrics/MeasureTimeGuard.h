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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <chrono>
#include "Histogram.h"

namespace arangodb::metrics
{
    template<typename ValueType>
    struct MeasureTimeGuard {
        explicit MeasureTimeGuard(
            metrics::HistogramBase<ValueType>& histogram) noexcept :
      _start(std::chrono::steady_clock::now()), _histogram(&histogram) {}
        MeasureTimeGuard(MeasureTimeGuard const&) = delete;
        MeasureTimeGuard(MeasureTimeGuard&&) = default;
        ~MeasureTimeGuard() {
          fire();
        }

        void fire() {
          if (_histogram) {
            auto const endTime = std::chrono::steady_clock::now();
            auto const duration =
                std::chrono::duration_cast<std::chrono::microseconds>(endTime - _start);
            _histogram->count(duration.count());
            _histogram.reset();
          }
        }

    private:
        std::chrono::steady_clock::time_point const _start;
        struct noop {
            template<typename T>
            void operator()(T*) {}
        };

        std::unique_ptr<HistogramBase<ValueType>, noop>
            _histogram;
    };
}