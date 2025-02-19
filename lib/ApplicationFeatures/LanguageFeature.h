////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Utf8Helper.h"

#include <unicode/locid.h>
#include <unicode/urename.h>

#include <memory>
#include <string>
#include <tuple>

namespace arangodb {
namespace application_features {
class GreetingsFeaturePhase;
}
namespace options {
class ProgramOptions;
}

class LanguageFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Language"; }

  template<typename Server>
  explicit LanguageFeature(Server& server)
      : application_features::ApplicationFeature{server, *this},
        _binaryPath(server.getBinaryPath()),
        _locale(),
        _langType(basics::LanguageType::INVALID),
        _forceLanguageCheck(true) {
    setOptional(false);
    startsAfter<application_features::GreetingsFeaturePhase, Server>();
  }

  ~LanguageFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;

  void prepare() override final;

  static std::string prepareIcu(std::string const& binaryPath,
                                std::string const& binaryExecutionPath,
                                std::string& path,
                                std::string const& binaryName);

  icu_64_64::Locale& getLocale();
  std::tuple<std::string_view, basics::LanguageType> getLanguage() const;
  bool forceLanguageCheck() const;
  std::string getCollatorLanguage() const;
  void resetLanguage(std::string_view language, basics::LanguageType type);

 private:
  char const* _binaryPath;
  std::string _icuData;
  icu_64_64::Locale _locale;
  std::string _defaultLanguage;
  std::string _icuLanguage;
  basics::LanguageType _langType;
  bool _forceLanguageCheck;
};

}  // namespace arangodb
