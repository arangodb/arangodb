////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "CacheOptionsFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Cache/Manager.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

CacheOptionsFeature::CacheOptionsFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();

  _options.cacheSize =
      (PhysicalMemory::getValue() >= (static_cast<std::uint64_t>(4) << 30))
          ? static_cast<std::uint64_t>((PhysicalMemory::getValue() -
                                        (static_cast<std::uint64_t>(2) << 30)) *
                                       0.25)
          : (256 << 20);
  // currently there is no way to turn stats off
  _options.enableWindowedStats = true;
}

void CacheOptionsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("cache", "in-memory hash cache");

  options
      ->addOption("--cache.size",
                  "The global size limit for all caches (in bytes).",
                  new UInt64Parameter(&_options.cacheSize),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Dynamic))
      .setLongDescription(R"(The global caching system, all caches, and all the
data contained therein are constrained to this limit.

If there is less than 4 GiB of RAM in the system, default value is 256 MiB.
If there is more, the default is `(system RAM size - 2 GiB) * 0.25`.)");

  options
      ->addOption(
          "--cache.rebalancing-interval",
          "The time between cache rebalancing attempts (in microseconds). "
          "The minimum value is 500000 (0.5 seconds).",
          new UInt64Parameter(&_options.rebalancingInterval, /*base*/ 1,
                              /*minValue*/ minRebalancingInterval))
      .setLongDescription(R"(The server uses a cache system which pools memory
across many different cache tables. In order to provide intelligent internal
memory management, the system periodically reclaims memory from caches which are
used less often and reallocates it to caches which get more activity.)");

  options
      ->addOption(
          "--cache.ideal-lower-fill-ratio",
          "The lower bound fill ratio value for a cache table.",
          new DoubleParameter(&_options.idealLowerFillRatio, /*base*/ 1.0,
                              /*minValue*/ 0.0, /*maxValue*/ 1.0),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Cache tables with a fill ratio lower than this
value will be shrunk by the cache rebalancer.)")
      .setIntroducedIn(31102);

  options
      ->addOption(
          "--cache.ideal-upper-fill-ratio",
          "The upper bound fill ratio value for a cache table.",
          new DoubleParameter(&_options.idealUpperFillRatio, /*base*/ 1.0,
                              /*minValue*/ 0.0, /*maxValue*/ 1.0),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Cache tables with a fill ratio higher than this
value will be inflated in size by the cache rebalancer.)")
      .setIntroducedIn(31102);

  options
      ->addOption("--cache.min-value-size-for-edge-compression",
                  "The size threshold (in bytes) from which on payloads in the "
                  "edge index cache transparently get LZ4-compressed.",
                  new SizeTParameter(&_options.minValueSizeForEdgeCompression,
                                     1, 0, 1073741824ULL),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(
          R"(By transparently compressing values in the in-memory
edge index cache, more data can be held in memory than without compression.  
Storing compressed values can increase CPU usage for the on-the-fly compression 
and decompression. In case compression is undesired, this option can be set to a 
very high value, which will effectively disable it. To use compression, set the
option to a value that is lower than medium-to-large average payload sizes.
It is normally not that useful to compress values that are smaller than 100 bytes.)")
      .setIntroducedIn(31102);

  options
      ->addOption(
          "--cache.acceleration-factor-for-edge-compression",
          "The acceleration factor for the LZ4 compression of in-memory "
          "edge cache entries.",
          new UInt32Parameter(&_options.accelerationFactorForEdgeCompression, 1,
                              1, 65537),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(
          R"(This value controls the LZ4-internal acceleration factor for the 
LZ4 compression. Higher values typically yield less compression in exchange
for faster compression and decompression speeds. An increase of 1 commonly leads
to a compression speed increase of 3%, and could slightly increase decompression
speed.)")
      .setIntroducedIn(31102);

  options
      ->addOption(
          "--cache.max-spare-memory-usage",
          "The maximum memory usage for spare tables in the in-memory cache.",
          new UInt64Parameter(&_options.maxSpareAllocation),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31103);
}

void CacheOptionsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {
  if (_options.cacheSize > 0 && _options.cacheSize < cache::Manager::kMinSize) {
    LOG_TOPIC("75778", FATAL, arangodb::Logger::FIXME)
        << "invalid value for `--cache.size', need at least "
        << cache::Manager::kMinSize;
    FATAL_ERROR_EXIT();
  }

  if (_options.idealLowerFillRatio >= _options.idealUpperFillRatio) {
    LOG_TOPIC("5fd67", FATAL, arangodb::Logger::FIXME)
        << "invalid values for `--cache.ideal-lower-fill-ratio' and "
           "`--cache.ideal-upper-fill-ratio`";
    FATAL_ERROR_EXIT();
  }
}

CacheOptions CacheOptionsFeature::getOptions() const { return _options; }

}  // namespace arangodb
