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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RandomFeature.h"

#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

RandomFeature::RandomFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Random"),
      _randomGenerator((uint32_t)RandomGenerator::RandomType::MERSENNE) {
  setOptional(false);
  startsAfter<LoggerFeature>();
}

void RandomFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("random", "Configure the random generator");

#ifdef _WIN32
  std::unordered_set<uint32_t> generators = {1, 5};
#else
  std::unordered_set<uint32_t> generators = {1, 2, 3, 4};
#endif

  options->addOption(
      "--random.generator",
      "random number generator to use (1 = MERSENNE, 2 = RANDOM, "
      "3 = URANDOM, 4 = COMBINED (not for Windows), 5 = WinCrypt (Windows "
      "only)",
      new DiscreteValuesParameter<UInt32Parameter>(&_randomGenerator, generators),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

void RandomFeature::prepare() {
  RandomGenerator::initialize((RandomGenerator::RandomType)_randomGenerator);
}

}  // namespace arangodb
