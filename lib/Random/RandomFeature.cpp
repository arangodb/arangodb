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

#include "RandomFeature.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

namespace arangodb {

RandomFeature::RandomFeature(application_features::ApplicationServer& server,
                             size_t registration)
    : ApplicationFeature(server, registration, name()),
      _randomGenerator((uint32_t)RandomGenerator::RandomType::MERSENNE) {
  setOptional(false);
}

void RandomFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("random", "random generator");

  std::unordered_set<uint32_t> generators = {1, 2, 3, 4};

  options
      ->addOption(
          "--random.generator",
          "The random number generator to use (1 = MERSENNE, 2 = RANDOM, "
          "3 = URANDOM, 4 = COMBINED). The options 2, 3, and 4 are deprecated "
          "and will be removed in a future version.",
          new DiscreteValuesParameter<UInt32Parameter>(&_randomGenerator,
                                                       generators),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(- `1`: a pseudo-random number generator using an
implication of the Mersenne Twister MT19937 algorithm
- `2`: use a blocking random (or pseudo-random) number generator
- `3`: use the non-blocking random (or pseudo-random) number generator supplied
  by the operating system
- `4`: a combination of the blocking random number generator and the Mersenne
  Twister)");
}

void RandomFeature::prepare() {
  RandomGenerator::initialize((RandomGenerator::RandomType)_randomGenerator);
}

}  // namespace arangodb
