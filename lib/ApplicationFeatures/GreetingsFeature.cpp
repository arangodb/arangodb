////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "GreetingsFeature.h"
#include "Logger/Logger.h"
#include "Rest/Version.h"

#include <iostream>

namespace arangodb {

GreetingsFeature::GreetingsFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "Greetings") {
  setOptional(false);
  startsAfter("Logger");
}

void GreetingsFeature::prepare() {
#ifdef __linux__
  if (isatty(STDOUT_FILENO)) {
    std::cout << "\x1b[0;1;35;95m╔═\x1b[0;1;31;91m║╔\x1b[0;1;33;93m═║\x1b[0;1;32;92m╔═\x1b[0;1;36;96m║╔\x1b[0;1;34;94m═\x1b[0m \x1b[0;1;35;95m╔═\x1b[0;1;31;91m╝╔\x1b[0;1;33;93m═║\x1b[0;1;32;92m╔═\x1b[0m \x1b[0;1;36;96m╔\x1b[0;1;34;94m═\x1b[0m   \x1b[0;1;31;91m══\x1b[0;1;33;93m║\x1b[0m \x1b[0;1;32;92m║\x1b[0m \x1b[0;1;36;96m║\x1b[0m  \x1b[0;1;34;94m╔\x1b[0;1;35;95m═║\x1b[0;1;31;91m╔═\x1b[0;1;33;93m╝═\x1b[0;1;32;92m═║\x1b[0m" << std::endl;
    std::cout << "\x1b[0;1;31;91m╔═\x1b[0;1;33;93m║╔\x1b[0;1;32;92m╔╝\x1b[0;1;36;96m╔═\x1b[0;1;34;94m║║\x1b[0m \x1b[0;1;35;95m║\x1b[0;1;31;91m║\x1b[0m \x1b[0;1;33;93m║║\x1b[0m \x1b[0;1;32;92m║\x1b[0;1;36;96m║\x1b[0m \x1b[0;1;34;94m║╔\x1b[0;1;35;95m═║\x1b[0m  \x1b[0;1;33;93m══\x1b[0;1;32;92m║\x1b[0m \x1b[0;1;36;96m══\x1b[0;1;34;94m║═\x1b[0;1;35;95m╝╔\x1b[0;1;31;91m╔╝\x1b[0;1;33;93m║\x1b[0m  \x1b[0;1;32;92m╔\x1b[0;1;36;96m═╝\x1b[0m" << std::endl;
    std::cout << "\x1b[0;1;33;93m╝\x1b[0m \x1b[0;1;32;92m╝╝\x1b[0m \x1b[0;1;36;96m╝\x1b[0;1;34;94m╝\x1b[0m \x1b[0;1;35;95m╝╝\x1b[0m \x1b[0;1;31;91m╝\x1b[0;1;33;93m══\x1b[0;1;32;92m╝═\x1b[0;1;36;96m═╝\x1b[0;1;34;94m══\x1b[0m \x1b[0;1;35;95m═\x1b[0;1;31;91m═\x1b[0m   \x1b[0;1;32;92m══\x1b[0;1;36;96m╝╝\x1b[0m  \x1b[0;1;35;95m╝\x1b[0m  \x1b[0;1;31;91m╝\x1b[0m \x1b[0;1;33;93m╝\x1b[0;1;32;92m══\x1b[0;1;36;96m╝═\x1b[0;1;34;94m═╝\x1b[0m" << std::endl;
    std::cout << std::endl;
  }
#endif
  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "" << rest::Version::getVerboseVersionString();
}

void GreetingsFeature::unprepare() {
  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "ArangoDB has been shut down";
}

} // arangodb
