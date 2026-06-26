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

#include "Network/NetworkOptionsProvider.h"

#include "Basics/StaticStrings.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void NetworkOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, NetworkOptions& opts) {
  options->addSection("network", "cluster-internal networking");

  options->addOption(
      "--network.io-threads",
      "The number of network I/O threads for cluster-internal "
      "communication.",
      new UInt32Parameter(&opts.numIOThreads, /*base*/ 1, /*minValue*/ 1));
  options->addOption("--network.max-open-connections",
                     "The maximum number of open TCP connections for "
                     "cluster-internal communication per endpoint",
                     new UInt64Parameter(&opts.maxOpenConnections,
                                         /*base*/ 1, /*minValue*/ 8));
  options->addOption("--network.idle-connection-ttl",
                     "The default time-to-live of idle connections for "
                     "cluster-internal communication (in milliseconds).",
                     new UInt64Parameter(&opts.idleTtlMilli));
  options->addOption(
      "--network.verify-hosts",
      "Verify peer certificates when using TLS in cluster-internal "
      "communication.",
      new BooleanParameter(&opts.verifyHosts));

  std::unordered_set<std::string> protos = {"", "http", "http2", "h2"};

  // starting with 3.9, we will hard-code the protocol for cluster-internal
  // communication
  options
      ->addOption(
          "--network.protocol",
          "The network protocol to use for cluster-internal communication.",
          new DiscreteValuesParameter<StringParameter>(&opts.protocol, protos),
          options::makeDefaultFlags(options::Flags::Uncommon))
      .setDeprecatedIn(30900);

  options
      ->addOption("--network.max-requests-in-flight",
                  "The number of internal requests that can be in "
                  "flight at a given point in time.",
                  new options::UInt64Parameter(&opts.maxInFlight),
                  options::makeDefaultFlags(options::Flags::Uncommon))
      .setIntroducedIn(30800);

  options
      ->addOption("--network.compress-request-threshold",
                  "The HTTP request body size from which on cluster-internal "
                  "requests are transparently compressed.",
                  new UInt64Parameter(&opts.compressRequestThreshold),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Automatically compress outgoing HTTP requests in cluster-internal
traffic with the deflate, gzip or lz4 compression format.
Compression will only happen if the size of the uncompressed request body
exceeds the threshold value controlled by this startup option,
and if the request body size after compression is less than the original
request body size.
Using the value 0 disables the automatic compression.)");

  std::unordered_set<std::string> types = {
      StaticStrings::EncodingGzip, StaticStrings::EncodingDeflate,
      StaticStrings::EncodingLz4, "auto", "none"};
  options
      ->addOption("--network.compression-method",
                  "The compression method used for cluster-internal requests.",
                  new DiscreteValuesParameter<StringParameter>(
                      &opts.compressionTypeLabel, types),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Setting this option to 'none' will disable compression for
cluster-internal requests.
To enable compression for cluster-internal requests, set this option to either
'deflate', 'gzip', 'lz4' or 'auto'.
The 'deflate' and 'gzip' compression methods are general purpose,
but have significant CPU overhead for performing the compression work.
The 'lz4' compression method compresses slightly worse, but has a lot lower
CPU overhead for performing the compression.
The 'auto' compression method will use 'deflate' by default, and 'lz4' for
requests which have a size that is at least 3 times the configured threshold
size.
The compression method only matters if `--network.compress-request-threshold`
is set to value greater than zero. If the threshold is set to value of 0,
then no compression will be performed.)");
}

void NetworkOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, NetworkOptions& options) {
  if (options.idleTtlMilli < 10000) {
    options.idleTtlMilli = 10000;
  }

  uint64_t clamped =
      std::clamp(options.maxInFlight, NetworkOptions::MinAllowedInFlight,
                 NetworkOptions::MaxAllowedInFlight);
  if (clamped != options.maxInFlight) {
    LOG_TOPIC("38cd1", WARN, Logger::CONFIG)
        << "Must set --network.max-requests-in-flight between "
        << NetworkOptions::MinAllowedInFlight << " and "
        << NetworkOptions::MaxAllowedInFlight << ", clamping value";
    options.maxInFlight = clamped;
  }

  if (options.compressionTypeLabel == StaticStrings::EncodingGzip) {
    options.compressionType = NetworkOptions::CompressionType::kGzip;
  } else if (options.compressionTypeLabel == StaticStrings::EncodingDeflate) {
    options.compressionType = NetworkOptions::CompressionType::kDeflate;
  } else if (options.compressionTypeLabel == StaticStrings::EncodingLz4) {
    options.compressionType = NetworkOptions::CompressionType::kLz4;
  } else if (options.compressionTypeLabel == "auto") {
    options.compressionType = NetworkOptions::CompressionType::kAuto;
  } else if (options.compressionTypeLabel == "none") {
    options.compressionType = NetworkOptions::CompressionType::kNone;
  } else {
    LOG_TOPIC("339d5", FATAL, Logger::CONFIG)
        << "invalid value for `--network.compression-method` ('"
        << options.compressionTypeLabel << "')";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
