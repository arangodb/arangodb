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

#include "LanguageFeature.h"

#include "Basics/FileUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/application-exit.h"
#include "Basics/error.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "Basics/memory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <absl/strings/str_cat.h>
#include <cstdlib>

namespace {
void setCollator(std::string_view language,
                 arangodb::basics::LanguageType type) {
  using arangodb::basics::Utf8Helper;

  switch (type) {
    case arangodb::basics::LanguageType::DEFAULT:
      LOG_TOPIC("e5954", DEBUG, arangodb::Logger::CONFIG)
          << "setting collator language for default to '" << language << "'";
      break;
    case arangodb::basics::LanguageType::ICU:
      LOG_TOPIC("a4667", DEBUG, arangodb::Logger::CONFIG)
          << "setting collator language for ICU to '" << language << "'";
      break;
    default:
      break;
  }

  if (!Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(language, type)) {
    LOG_TOPIC("01490", FATAL, arangodb::Logger::FIXME)
        << "error setting collator language to '" << language << "'. "
        << "The icudtl.dat file might be of the wrong version. "
        << "Check for an incorrectly set ICU_DATA environment variable";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_ICU_INITIALIZATION_FAILED);
  }
}

void setLocale(icu_64_64::Locale& locale) {
  using arangodb::basics::Utf8Helper;
  std::string languageName;

  if (!Utf8Helper::DefaultUtf8Helper.getCollatorCountry().empty()) {
    languageName =
        absl::StrCat(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage(), "_",
                     Utf8Helper::DefaultUtf8Helper.getCollatorCountry());
    locale = icu_64_64::Locale(
        Utf8Helper::DefaultUtf8Helper.getCollatorLanguage().c_str(),
        Utf8Helper::DefaultUtf8Helper.getCollatorCountry().c_str()
        /*
           const   char * variant  = 0,
           const   char * keywordsAndValues = 0
        */
    );
  } else {
    locale = icu_64_64::Locale(
        Utf8Helper::DefaultUtf8Helper.getCollatorLanguage().c_str());
    languageName = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
  }

  LOG_TOPIC("f6e04", DEBUG, arangodb::Logger::CONFIG)
      << "using default language '" << languageName << "'";
}

arangodb::basics::LanguageType getLanguageType(
    std::string_view default_lang, std::string_view icu_lang) noexcept {
  if (icu_lang.empty()) {
    return arangodb::basics::LanguageType::DEFAULT;
  }
  if (default_lang.empty()) {
    return arangodb::basics::LanguageType::ICU;
  }
  return arangodb::basics::LanguageType::INVALID;
}

}  // namespace

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

LanguageFeature::~LanguageFeature() = default;

void LanguageFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options
      ->addOption("--default-language",
                  "An ISO-639 language code. You can only set this option "
                  "once, when initializing the database.",
                  new StringParameter(&_defaultLanguage),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(31000)
      .setLongDescription(R"(The default language is used for sorting and
comparing strings. The language value is a two-letter language code (ISO-639) or
it is composed by a two-letter language code followed by a two letter country
code (ISO-3166). For example: `de`, `en`, `en_US`, `en_UK`.

The default is the system locale of the platform.)");

  options
      ->addOption(
          "--icu-language",
          "An ICU locale ID to set a language and optionally additional "
          "properties that affect string comparisons and sorting. You can only "
          "set this option once, when initializing the database.",
          new StringParameter(&_icuLanguage),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30901)
      .setLongDescription(R"(With this option, you can get the sorting and
comparing order exactly as it is defined in the ICU standard. The language value
can be a two-letter language code (ISO-639), a two-letter language code followed
by a two letter country code (ISO-3166), or any other valid ICU locale
definition. For example: `de`, `en`, `en_US`, `en_UK`,
`de_AT@collation=phonebook`.

For the Swedish language (`sv`), for instance, the correct ICU-based sorting
order for letters is `'a','A','b','B','z','Z','å','Ä','ö','Ö'`. To get this
order, use `--icu-language sv`. If you use `--default-language sv` instead, the
sorting order will be `"A", "a", "B", "b", "Z", "z", "å", "Ä", "Ö", "ö"`.

**Note**: You can use only one of the language options, either `--icu-language`
or `--default-language`. Setting both of them results in an error.)");

  options
      ->addOption(
          "--default-language-check",
          "Check if `--icu-language` / `--default-language` matches the "
          "stored language.",
          new BooleanParameter(&_forceLanguageCheck),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800);
}

void LanguageFeature::prepare() {
  _langType = ::getLanguageType(_defaultLanguage, _icuLanguage);

  if (LanguageType::INVALID == _langType) {
    LOG_TOPIC("d8a99", FATAL, arangodb::Logger::CONFIG)
        << "Only one parameter from --default-language and --icu-language "
           "should be specified";
    FATAL_ERROR_EXIT();
  }

  ::setCollator(
      _langType == LanguageType::ICU ? _icuLanguage : _defaultLanguage,
      _langType);
}

void LanguageFeature::start() { ::setLocale(_locale); }

icu_64_64::Locale& LanguageFeature::getLocale() { return _locale; }

std::tuple<std::string_view, LanguageType> LanguageFeature::getLanguage()
    const {
  if (LanguageType::ICU == _langType) {
    return {_icuLanguage, _langType};
  }
  TRI_ASSERT(LanguageType::DEFAULT == _langType);
  // If it is invalid type just returning _defaultLanguage
  return {_defaultLanguage, _langType};
}

bool LanguageFeature::forceLanguageCheck() const { return _forceLanguageCheck; }

std::string LanguageFeature::getCollatorLanguage() const {
  using arangodb::basics::Utf8Helper;
  return Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
}

void LanguageFeature::resetLanguage(std::string_view language,
                                    arangodb::basics::LanguageType type) {
  _langType = type;
  _defaultLanguage.clear();
  _icuLanguage.clear();
  switch (_langType) {
    case LanguageType::DEFAULT:
      _defaultLanguage = language;
      break;

    case LanguageType::ICU:
      _icuLanguage = language;
      break;

    case LanguageType::INVALID:
    default:
      TRI_ASSERT(false);
      return;
  }

  ::setCollator(language.data(), _langType);
  ::setLocale(_locale);
}

}  // namespace arangodb
