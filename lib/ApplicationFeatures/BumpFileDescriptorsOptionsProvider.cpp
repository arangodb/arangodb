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

#include "BumpFileDescriptorsOptionsProvider.h"

#include "Assertions/ProdAssert.h"
#include "Basics/FileDescriptors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"

namespace arangodb {

void BumpFileDescriptorsOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> prgOpts,
    BumpFileDescriptorsFeatureOptions& bfdOpts) {
  // An empty name would silently register an unreachable option.
  ADB_PROD_ASSERT(!bfdOpts.optionName.empty())
      << "setOptionName() must be called before options are declared";
  prgOpts
      ->addOption(
          bfdOpts.optionName,
          "The minimum number of file descriptors needed to start (0 = no "
          "minimum)",
          new options::UInt64Parameter(&bfdOpts.descriptorsMinimum),
          arangodb::options::makeFlags())
      .setIntroducedIn(31200);
}

void BumpFileDescriptorsOptionsProvider::validateOptionsImpl(
    std::shared_ptr<options::ProgramOptions> /*prgOpts*/,
    BumpFileDescriptorsFeatureOptions& bfdOpts) {
  if (bfdOpts.descriptorsMinimum > 0 &&
      (bfdOpts.descriptorsMinimum < FileDescriptors::requiredMinimum ||
       bfdOpts.descriptorsMinimum > FileDescriptors::maximumValue)) {
    LOG_TOPIC("7e15c", FATAL, Logger::STARTUP)
        << "invalid value for " << bfdOpts.optionName << ". must be between "
        << FileDescriptors::requiredMinimum << " and "
        << FileDescriptors::maximumValue;
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
