////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "FileSystemFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/files.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::options;

namespace arangodb {

void FileSystemFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
#ifdef __linux__
  // option is only available on Linux
  options
      ->addOption("--use-splice-syscall",
                  "Use the splice() syscall for file copying (may not be "
                  "supported on all filesystems).",
                  new BooleanParameter(&_useSplice),
                  options::makeFlags(Flags::DefaultNoOs, Flags::OsLinux))
      .setIntroducedIn(30904)
      .setLongDescription(R"(While the syscall is generally available since
Linux 2.6.x, it is also required that the underlying filesystem supports the
splice operation. This is not true for some encrypted filesystems
(e.g. ecryptfs), on which `splice()` calls can fail.

You can set the `--use-splice-syscall` startup option to `false` to use a less
efficient, but more portable file copying method instead, which should work on
all filesystems.)");
#endif
}

void FileSystemFeature::prepare() {
#ifdef __linux__
  TRI_SetCanUseSplice(_useSplice);
#endif
}

}  // namespace arangodb
