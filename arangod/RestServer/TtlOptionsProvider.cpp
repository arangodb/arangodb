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

#include "TtlOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/TtlFeature.h"

namespace arangodb {

using namespace arangodb::options;

void TtlOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> options, TtlProperties& props) {
  options->addSection("ttl", "TTL index options");

  options
      ->addOption(
          "--ttl.frequency",
          "The frequency (in milliseconds) for the TTL background thread "
          "invocation (0 = turn the TTL background thread off entirely).",
          new UInt64Parameter(&props.frequency))
      .setLongDescription(R"(The lower this value, the more frequently the TTL
background thread kicks in and scans all available TTL indexes for expired
documents, and the earlier the expired documents are actually removed.)");

  options
      ->addOption("--ttl.max-total-removes",
                  "The maximum number of documents to remove per invocation of "
                  "the TTL thread.",
                  new UInt64Parameter(&props.maxTotalRemoves, /*base*/ 1,
                                      /*minValue*/ 1))
      .setLongDescription(R"(In order to avoid "random" load spikes by the
background thread suddenly kicking in and removing a lot of documents at once,
you can cap the number of to-be-removed documents per thread invocation.

The TTL background thread goes back to sleep once it has removed the configured
number of documents in one iteration. If more candidate documents are left for
removal, they are removed in subsequent runs of the background thread.)");

  options
      ->addOption(
          "--ttl.max-collection-removes",
          "The maximum number of documents to remove per collection in each "
          "invocation of the TTL thread.",
          new UInt64Parameter(&props.maxCollectionRemoves, /*base*/ 1,
                              /*minValue*/ 1))
      .setLongDescription(R"(You can configure this value separately from the
total removal amount so that the per-collection time window for locking and
potential write-write conflicts can be reduced.)");

  // the following option was obsoleted in 3.8
  options->addObsoleteOption(
      "--ttl.only-loaded-collection",
      "only consider already loaded collections for removal", false);
}

void TtlOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> /*options*/, TtlProperties& props) {
  if (props.maxCollectionRemoves == 0) {
    LOG_TOPIC("2ab82", FATAL, arangodb::Logger::STARTUP)
        << "invalid value for '--ttl.max-collection-removes'.";
    FATAL_ERROR_EXIT();
  }

  if (props.frequency > 0 && props.frequency < TtlProperties::minFrequency) {
    LOG_TOPIC("ea696", FATAL, arangodb::Logger::STARTUP)
        << "too low value for '--ttl.frequency'.";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
