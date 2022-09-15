////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::options;

namespace arangodb {

FileSystemFeature::FileSystemFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature(server, "FileSystem") {
  setOptional(false);
  startsAfter<LoggerFeature>();
}

void FileSystemFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
#ifdef __linux__
  // option is only available on Linux
  options
      ->addOption("--use-splice-syscall",
                  "Use the splice() syscall for file copying (may not be "
                  "supported on all filesystems",
                  new BooleanParameter(&_useSplice),
                  options::makeFlags(Flags::DefaultNoOs, Flags::OsLinux))
      .setIntroducedIn(30904);
#endif
}

void FileSystemFeature::prepare() {
#ifdef __linux__
  TRI_SetCanUseSplice(_useSplice);
#endif
}

}  // namespace arangodb
