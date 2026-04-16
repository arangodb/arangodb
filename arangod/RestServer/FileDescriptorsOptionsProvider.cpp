////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2026 ArangoDB GmbH, Hyderabad, India
/// Copyright 2026 triAGENS GmbH, Hyderabad, India
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
/// Copyright holder is ArangoDB GmbH, Hyderabad, India
///
////////////////////////////////////////////////////////////////////////////////

#include "FileDescriptorsOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::file_descriptors {

using namespace arangodb::options;

void FileDescriptorsOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options,
    FileDescriptorsFeatureOptions& opts) {
  options
      ->addOption(
          "--server.count-descriptors-interval",
          "Controls the interval (in milliseconds) in which the number of open "
          "file descriptors for the process is determined "
          "(0 = disable counting).",
          new UInt64Parameter(&opts.countDescriptorsInterval), makeFlags())
      .setIntroducedIn(31100);
}

}  // namespace arangodb::file_descriptors
