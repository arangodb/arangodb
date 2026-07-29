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

#include "V8PlatformOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void V8PlatformOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    V8PlatformFeatureOptions& opts) {
  options->addSection("javascript", "JavaScript engine and execution");

  options
      ->addOption("--javascript.v8-options", "Options to pass to V8.",
                  new VectorParameter<StringParameter>(&opts.v8Options),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can optionally pass arguments to the V8
JavaScript engine. The V8 engine runs with the default settings unless you
explicitly specify them. The options are forwarded to the V8 engine, which
parses them on its own. Passing invalid options may result in an error being
printed on stderr and the option being ignored.

You need to pass the options as one string, with V8 option names being prefixed
with two hyphens. Multiple options need to be separated by whitespace. To get
a list of all available V8 options, you can use the value `"--help"` as follows:

```
--javascript.v8-options="--help"
```

Another example of specific V8 options being set at startup:

```
--javascript.v8-options="--log --no-logfile-per-isolate --logfile=v8.log"
```

Names and features or usable options depend on the version of V8 being used, and
might change in the future if a different version of V8 is being used in
ArangoDB. Not all options offered by V8 might be sensible to use in the context
of ArangoDB. Use the specific options only if you are sure that they are not
harmful for the regular database operation.)");

  options->addOption("--javascript.v8-max-heap",
                     "The maximal heap size (in MiB).",
                     new UInt64Parameter(&opts.v8MaxHeap));
}

}  // namespace arangodb
