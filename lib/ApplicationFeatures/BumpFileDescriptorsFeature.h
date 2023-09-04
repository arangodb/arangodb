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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/operating-system.h"

#include <string>

#ifdef TRI_HAVE_GETRLIMIT
namespace arangodb {
class LoggerFeature;

namespace application_features {
class GreetingsFeaturePhase;
}

class BumpFileDescriptorsFeature
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "BumpFileDescriptors";
  }

  template<typename Server>
  explicit BumpFileDescriptorsFeature(Server& server, std::string optionName)
      : application_features::ApplicationFeature{server, *this},
        _optionName(std::move(optionName)),
        _descriptorsMinimum(0) {
    setOptional(false);
    startsAfter<application_features::GreetingsFeaturePhase, Server>();
    startsAfter<LoggerFeature, Server>();
  }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

 private:
  std::string const _optionName;

  uint64_t _descriptorsMinimum;
};

}  // namespace arangodb
#endif
