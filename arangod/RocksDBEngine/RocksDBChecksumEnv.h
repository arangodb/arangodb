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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////
/*
 *
 *
 * What I would do first is to derive a ChecksummingEnv from Env which overrides
only NewWritableFile and prints a message. Then use that in the community case
instead of the default Env. The enterprise code can tell you how to use a
non-default Env.

neunhoef:house_with_garden:  10 hours ago
That would be the "foot in the door". Then we must inherit from WritableFile and
override enough methods to compute the right checksum, and then on "Close" of
the WritableFile we spit out the right hash file.

neunhoef:house_with_garden:  10 hours ago
Once this works for the community edition, we use that ChecksummingEnv between
the EncryptingEnv and the DefaultEnv to compute the right checksum there.


 */

#pragma once

#include <rocksdb/env.h>

namespace rocksdb {
class ChecksumEnv
    : public Env,
      public WritableFile {  // must mix it with Env::Default() for the moment
  Status NewWritableFile(const std::string& fname,
                         std::unique_ptr<WritableFile>* result,
                         const EnvOptions& options) override;
};
}  // namespace rocksdb