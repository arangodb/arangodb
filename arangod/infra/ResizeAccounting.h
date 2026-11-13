////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

namespace arangodb::infra {

// Accounting for a realloc, split into the amount that must be reserved on the
// new monitor before the resize and the amount released on the old monitor
// after. Both axes (requested/actual) are carried because the limit is
// enforced on `actual` while `requested` is bookkeeping.
struct ResizeDelta {
  uint64_t reserveRequested;
  uint64_t reserveActual;
  uint64_t releaseRequested;
  uint64_t releaseActual;
};

// Computes the accounting for resizing an allocation from (oldRequested,
// oldActual) to (newRequested, newActual).
//
// Across monitors the new monitor accounts for the full new size and the old
// monitor releases the full old size. When the allocation stays on the same
// monitor its old size is still counted, so the resize is netted per axis:
// reserve only the growth (the part that must fit under the limit) and release
// only the shrink. Reserving the full new size on top of the still-counted old
// size would spuriously trip the limit for a tiny increase, or even a shrink.
//
// The two axes are netted independently because they can move in opposite
// directions: replacing an over-allocated aligned block with a tight normal
// block grows `requested` yet shrinks `actual`.
constexpr auto computeResizeDelta(bool sameMonitor, uint64_t oldRequested,
                                  uint64_t oldActual, uint64_t newRequested,
                                  uint64_t newActual) -> ResizeDelta {
  if (!sameMonitor) {
    return {newRequested, newActual, oldRequested, oldActual};
  }
  return {
      newRequested > oldRequested ? newRequested - oldRequested : 0,
      newActual > oldActual ? newActual - oldActual : 0,
      oldRequested > newRequested ? oldRequested - newRequested : 0,
      oldActual > newActual ? oldActual - newActual : 0,
  };
}

}  // namespace arangodb::infra
