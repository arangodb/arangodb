////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "RestServer/FortuneFeature.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomGenerator.h"
#include "RestServer/BootstrapFeature.h"

using namespace arangodb;
using namespace arangodb::options;

namespace {

static char const* cookies[] = {
    "The number of computer scientists in a room is inversely proportional to "
    "the number of bugs in their code.",
    "Unnamed Law: If it happens, it must be possible.",
    "Of course there's no reason for it, it's just our policy.",
    "Your mode of life will be changed to ASCII.",
    "My program works if i take out the bugs",
    "Your lucky number has been disconnected.",
    "Any sufficiently advanced bug is indistinguishable from a feature.",
    "Real Users hate Real Programmers.",
    "Reality is just a crutch for people who can't handle science fiction.",
    "You're definitely on their list.  The question to ask next is what list "
    "it is.",
    "Any given program will expand to fill available memory.",
    "Steinbach's Guideline for Systems Programming: Never test for an error "
    "condition you don't know how to handle.",
    "Bug, n: A son of a glitch.",
    "Recursion n.: See Recursion.",
    "I think we're in trouble.  -- Han Solo",
    "18,446,744,073,709,551,616 is a big number",
    "Civilization, as we know it, will end sometime this evening. See SYSNOTE "
    "tomorrow for more information.",
    "Everything ends badly.  Otherwise it wouldn't end.",
    "The moon may be smaller than Earth, but it's further away.",
    "Never make anything simple and efficient when a way can be found to make "
    "it complex and wonderful.",
    ""};

}  // namespace

FortuneFeature::FortuneFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Fortune"), _fortune(false) {
  startsAfter<BootstrapFeature>();
}

void FortuneFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("fortune", "show fortune cookie on startup",
                     new BooleanParameter(&_fortune),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

void FortuneFeature::start() {
  if (!_fortune) {
    return;
  }

  uint32_t r = RandomGenerator::interval(
      static_cast<uint32_t>(sizeof(::cookies) / sizeof(::cookies)[0]));
  if (strlen(::cookies[r]) > 0) {
    LOG_TOPIC("f3422", INFO, Logger::FIXME) << ::cookies[r];
  }
}
