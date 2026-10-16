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
///
////////////////////////////////////////////////////////////////////////////////

#include "QueryRegistryFeatureOptions.h"

#include "Basics/PhysicalMemory.h"

#include <algorithm>

namespace arangodb {

namespace {

uint64_t defaultMemoryLimit(uint64_t available, double reserveFraction,
                            double percentage) {
  if (available == 0) {
    // we don't know how much memory is available, so we cannot do any sensible
    // calculation
    return 0;
  }

  // this function will produce the following results for a reserveFraction of
  // 0.2 and a percentage of 0.75 for some common available memory values:
  //
  //    Available memory:            0      (0MiB)  Limit:            0
  //    unlimited, %mem:  n/a Available memory:    134217728    (128MiB)  Limit:
  //    33554432     (32MiB), %mem: 25.0 Available memory:    268435456 (256MiB)
  //    Limit:     67108864     (64MiB), %mem: 25.0 Available memory: 536870912
  //    (512MiB)  Limit:    201326592    (192MiB), %mem: 37.5 Available memory:
  //    805306368    (768MiB)  Limit:    402653184    (384MiB), %mem: 50.0
  //    Available memory:   1073741824   (1024MiB)  Limit:    603979776
  //    (576MiB), %mem: 56.2 Available memory:   2147483648   (2048MiB)  Limit:
  //    1288490189   (1228MiB), %mem: 60.0 Available memory:   4294967296
  //    (4096MiB)  Limit:   2576980377   (2457MiB), %mem: 60.0 Available memory:
  //    8589934592   (8192MiB)  Limit:   5153960755   (4915MiB), %mem: 60.0
  //    Available memory:  17179869184  (16384MiB)  Limit:  10307921511
  //    (9830MiB), %mem: 60.0 Available memory:  25769803776  (24576MiB)  Limit:
  //    15461882265  (14745MiB), %mem: 60.0 Available memory:  34359738368
  //    (32768MiB)  Limit:  20615843021  (19660MiB), %mem: 60.0 Available
  //    memory:  42949672960  (40960MiB)  Limit:  25769803776  (24576MiB),
  //    %mem: 60.0 Available memory:  68719476736  (65536MiB)  Limit:
  //    41231686041  (39321MiB), %mem: 60.0 Available memory: 103079215104
  //    (98304MiB)  Limit:  61847529063  (58982MiB), %mem: 60.0 Available
  //    memory: 137438953472 (131072MiB)  Limit:  82463372083  (78643MiB),
  //    %mem: 60.0 Available memory: 274877906944 (262144MiB)  Limit:
  //    164926744167 (157286MiB), %mem: 60.0 Available memory: 549755813888
  //    (524288MiB)  Limit: 329853488333 (314572MiB), %mem: 60.0

  // for a reserveFraction of 0.05 and a percentage of 0.95 it will produce:
  //
  //    Available memory:            0      (0MiB)  Limit:            0
  //    unlimited, %mem:  n/a Available memory:    134217728    (128MiB)  Limit:
  //    33554432     (32MiB), %mem: 25.0 Available memory:    268435456 (256MiB)
  //    Limit:     67108864     (64MiB), %mem: 25.0 Available memory: 536870912
  //    (512MiB)  Limit:    255013683    (243MiB), %mem: 47.5 Available memory:
  //    805306368    (768MiB)  Limit:    510027366    (486MiB), %mem: 63.3
  //    Available memory:   1073741824   (1024MiB)  Limit:    765041049
  //    (729MiB), %mem: 71.2 Available memory:   2147483648   (2048MiB)  Limit:
  //    1785095782   (1702MiB), %mem: 83.1 Available memory:   4294967296
  //    (4096MiB)  Limit:   3825205248   (3648MiB), %mem: 89.0 Available memory:
  //    8589934592   (8192MiB)  Limit:   7752415969   (7393MiB), %mem: 90.2
  //    Available memory:  17179869184  (16384MiB)  Limit:  15504831938
  //    (14786MiB), %mem: 90.2 Available memory:  25769803776  (24576MiB) Limit:
  //    23257247908  (22179MiB), %mem: 90.2 Available memory:  34359738368
  //    (32768MiB)  Limit:  31009663877  (29573MiB), %mem: 90.2 Available
  //    memory:  42949672960  (40960MiB)  Limit:  38762079846  (36966MiB),
  //    %mem: 90.2 Available memory:  68719476736  (65536MiB)  Limit:
  //    62019327755  (59146MiB), %mem: 90.2 Available memory: 103079215104
  //    (98304MiB)  Limit:  93028991631  (88719MiB), %mem: 90.2 Available
  //    memory: 137438953472 (131072MiB)  Limit: 124038655509 (118292MiB),
  //    %mem: 90.2 Available memory: 274877906944 (262144MiB)  Limit:
  //    248077311017 (236584MiB), %mem: 90.2 Available memory: 549755813888
  //    (524288MiB)  Limit: 496154622034 (473169MiB), %mem: 90.2

  // reserveFraction% of RAM will be considered as a reserve
  uint64_t reserve = static_cast<uint64_t>(available * reserveFraction);

  // minimum reserve memory is 256MB
  reserve = std::max<uint64_t>(reserve, static_cast<uint64_t>(256) << 20);

  TRI_ASSERT(available > 0);
  double f = double(1.0) - (double(reserve) / double(available));
  double dyn = (double(available) * f * percentage);
  if (dyn < 0.0) {
    dyn = 0.0;
  }

  return std::max(static_cast<uint64_t>(dyn),
                  static_cast<uint64_t>(0.25 * available));
}

}  // namespace

QueryRegistryFeatureOptions::QueryRegistryFeatureOptions()
    : queryGlobalMemoryLimit(
          defaultMemoryLimit(PhysicalMemory::getValue(), 0.1, 0.90)),
      queryMemoryLimit(
          defaultMemoryLimit(PhysicalMemory::getValue(), 0.2, 0.75)) {}

}  // namespace arangodb
