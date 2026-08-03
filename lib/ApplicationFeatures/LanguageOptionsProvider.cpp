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

#include "LanguageOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void LanguageOptionsProvider::declareOptions(
    std::shared_ptr<options::ProgramOptions> options,
    LanguageFeatureOptions& opts) {
  options
      ->addOption("--default-language",
                  "An ISO-639 language code. You can only set this option "
                  "once, when initializing the database.",
                  new StringParameter(&opts.defaultLanguage),
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
          new StringParameter(&opts.icuLanguage),
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
          new BooleanParameter(&opts.forceLanguageCheck),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30800);
}

}  // namespace arangodb
