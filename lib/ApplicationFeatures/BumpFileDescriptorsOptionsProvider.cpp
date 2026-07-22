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

#include "Basics/FileDescriptors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"

namespace arangodb {

using namespace arangodb::options;

BumpFileDescriptorsOptionsProvider::BumpFileDescriptorsOptionsProvider(
    std::string optionName)
    : _optionName(std::move(optionName)) {}

void BumpFileDescriptorsOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options,
    BumpFileDescriptorsFeatureOptions& opts) {
  options
      ->addOption(
          _optionName,
          "The minimum number of file descriptors needed to start (0 = no "
          "minimum)",
          new UInt64Parameter(&opts.descriptorsMinimum),
          arangodb::options::makeFlags())
      .setIntroducedIn(31200);
}

void BumpFileDescriptorsOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/,
    BumpFileDescriptorsFeatureOptions& opts) {
  if (opts.descriptorsMinimum > 0 &&
      (opts.descriptorsMinimum < FileDescriptors::requiredMinimum ||
       opts.descriptorsMinimum > FileDescriptors::maximumValue)) {
    LOG_TOPIC("7e15c", FATAL, Logger::STARTUP)
        << "invalid value for " << _optionName << ". must be between "
        << FileDescriptors::requiredMinimum << " and "
        << FileDescriptors::maximumValue;
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
