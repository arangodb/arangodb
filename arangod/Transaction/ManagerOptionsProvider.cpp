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

#include "ManagerOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::transaction {

using namespace arangodb::options;

void ManagerOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, ManagerFeatureOptions& opts) {
  options->addSection("transaction", "transactions");

  options->addOption(
      "--transaction.streaming-lock-timeout",
      "The lock timeout (in seconds) "
      "in case of parallel access to the same Stream Transaction.",
      new DoubleParameter(&opts.streamingLockTimeout),
      makeDefaultFlags(Flags::Uncommon));

  options
      ->addOption(
          "--transaction.streaming-idle-timeout",
          "The idle timeout (in seconds) for Stream Transactions.",
          new DoubleParameter(
              &opts.streamingIdleTimeout, /*base*/ 1.0,
              /*minValue*/ 0.0,
              /*maxValue*/ ManagerFeatureOptions::maxStreamingIdleTimeout),
          makeFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                    Flags::OnSingle))
      .setIntroducedIn(30800)
      .setLongDescription(R"(Stream Transactions automatically expire after
this period when no further operations are posted into them. Posting an
operation into a non-expired Stream Transaction resets the transaction's
timeout to the configured idle timeout.)");

  options
      ->addOption(
          "--transaction.streaming-max-transaction-size",
          "The maximum transaction size (in bytes) for Stream Transactions.",
          new SizeTParameter(&opts.streamingMaxTransactionSize),
          makeFlags(Flags::Uncommon, Flags::DefaultNoComponents,
                    Flags::OnDBServer, Flags::OnSingle))
      .setIntroducedIn(31200);
}

}  // namespace arangodb::transaction
