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

#include "PrivilegeOptionsProvider.h"

#include "Basics/operating-system.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::options;

namespace arangodb {

void PrivilegeOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, PrivilegeFeatureOptions& opts) {
#ifdef ARANGODB_HAVE_SETUID
  options
      ->addOption(
          "--uid",
          "Switch to this user ID after reading the configuration files.",
          new StringParameter(&opts.uid), makeDefaultFlags(Flags::Uncommon))
      .setLongDescription(R"(The name (identity) of the user to run the
server as.

If you don't specify this option, the server does not attempt to change its UID,
so that the UID used by the server is the same as the UID of the user who
started the server.

If you specify this option, the server changes its UID after opening ports and
reading configuration files, but before accepting connections or opening other
files (such as recovery files). This is useful if the server must be started
with raised privileges (in certain environments) but security considerations
require that these privileges are dropped once the server has started work.

**Note**: You cannot use this option to bypass operating system security.
In general, this option (and the related `--gid`) can lower privileges but not
raise them.)");

  options->addOption(
      "--server.uid",
      "Switch to this user ID after reading configuration files.",
      new StringParameter(&opts.uid), makeDefaultFlags(Flags::Uncommon));
#endif

#ifdef ARANGODB_HAVE_SETGID
  options
      ->addOption(
          "--gid", "Switch to this group ID after reading configuration files.",
          new StringParameter(&opts.gid), makeDefaultFlags(Flags::Uncommon))
      .setLongDescription(R"(The name (identity) of the group to run the
server as.

If you don't specify this option, the server does not attempt to change its GID,
so that the GID the server runs as is the primary group of the user who started
the server.

If you specify this option, the server changes its GID after opening ports and
reading configuration files, but before accepting connections or opening other
files (such as recovery files).)");

  options->addOption(
      "--server.gid",
      "Switch to this group ID after reading configuration files.",
      new StringParameter(&opts.gid), makeDefaultFlags(Flags::Uncommon));
#endif
}

}  // namespace arangodb
