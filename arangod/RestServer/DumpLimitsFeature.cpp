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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DumpLimitsFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {
uint64_t defaultMemoryUsage() {
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.2
    return static_cast<uint64_t>(
        (PhysicalMemory::getValue() - (static_cast<uint64_t>(2) << 30)) * 0.2);
  }
  // if we have at least 2GB of RAM, the default size is 64MB
  return (static_cast<uint64_t>(64) << 20);
}
}  // namespace

namespace arangodb {

DumpLimitsFeature::DumpLimitsFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();

  _dumpLimits.memoryUsage = defaultMemoryUsage();
}

void DumpLimitsFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("dump", "Dump limits");

  options
      ->addOption(
          "--dump.max-memory-usage",
          "Maximum memory usage (in bytes) to be used by all ongoing dumps.",
          new UInt64Parameter(&_dumpLimits.memoryUsage, 1,
                              /*minimum*/ 16 * 1024 * 1024),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Dynamic,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(The approximate per-server maximum allowed memory usage value
for all ongoing dump actions combined.)");

  options
      ->addOption(
          "--dump.max-docs-per-batch",
          "Maximum number of documents per batch that can be used in a dump.",
          new UInt64Parameter(&_dumpLimits.docsPerBatchUpperBound, 1,
                              /*minimum*/ _dumpLimits.docsPerBatchLowerBound),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Each batch in a dump can grow to at most this size.)");

  options
      ->addOption(
          "--dump.max-batch-size",
          "Maximum batch size value (in bytes) that can be used in a dump.",
          new UInt64Parameter(&_dumpLimits.batchSizeUpperBound, 1,
                              /*minimum*/ _dumpLimits.batchSizeLowerBound),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Each batch in a dump can grow to at most this size.)");

  options
      ->addOption(
          "--dump.max-parallelism",
          "Maximum parallelism that can be used in a dump.",
          new UInt64Parameter(&_dumpLimits.parallelismUpperBound, 1,
                              /*minimum*/ _dumpLimits.parallelismLowerBound),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(Each dump action on a server can use at most
this many parallel threads. Note that end users can still start multiple 
dump actions that run in parallel.)");
}

void DumpLimitsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  if (_dumpLimits.batchSizeLowerBound > _dumpLimits.batchSizeUpperBound) {
    LOG_TOPIC("79c1b", FATAL, arangodb::Logger::CONFIG)
        << "invalid value for --dump.max-batch-size. Please use a value "
        << "of at least " << _dumpLimits.batchSizeLowerBound;
    FATAL_ERROR_EXIT();
  }

  if (_dumpLimits.parallelismLowerBound > _dumpLimits.parallelismUpperBound) {
    LOG_TOPIC("f433c", FATAL, arangodb::Logger::CONFIG)
        << "invalid value for --dump.max-parallelism. Please use a value "
        << "of at least " << _dumpLimits.parallelismLowerBound;
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
