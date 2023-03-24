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

namespace arangodb {
namespace options {
class ProgramOptions;
}

class LoggerFeature;

class FileSystemFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "FileSystem"; }

  template<typename Server>
  explicit FileSystemFeature(Server& server)
      : ApplicationFeature{server, *this} {
    setOptional(false);
    startsAfter<LoggerFeature, Server>();
  }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

 private:
  // whether or not to use the splice() syscall on Linux
#ifdef __linux__
  bool _useSplice = true;
#endif
};

}  // namespace arangodb
