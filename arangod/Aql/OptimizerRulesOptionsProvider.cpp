////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include "OptimizerRulesOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::aql {

using namespace arangodb::options;

void OptimizerRulesOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, OptimizerRulesOptions& opts) {
  options
      ->addOption("--query.optimizer-rules",
                  "Enable or disable specific optimizer rules by default. "
                  "Specify the rule name prefixed with `-` for disabling, or "
                  "`+` for enabling.",
                  new VectorParameter<StringParameter>(&opts.optimizerRules),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can use this option to selectively enable or
disable AQL query optimizer rules by default. You can specify the option
multiple times.

For example, to turn off the rules `use-indexes-for-sort` and
`reduce-extraction-to-projection` by default, use the following:

```
--query.optimizer-rules "-use-indexes-for-sort" --query.optimizer-rules "-reduce-extraction-to-projection"
```

The purpose of this startup option is to be able to enable potential future
experimental optimizer rules, which may be shipped in a disabled-by-default
state.)");

  options
      ->addObsoleteOption(
          "--query.parallelize-gather-writes",
          "Whether to enable write parallelization for gather nodes.", false)
      .setLongDescription(
          "Starting with 3.11 almost all queries support parallelization of "
          "gather nodes, making this option obsolete.");
}

}  // namespace arangodb::aql
