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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <sys/types.h>
#include <memory>
#include <string>

#include "Basics/operating-system.h"

#include "RestServer/arangod.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}

class PrivilegeFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Privilege"; }

  explicit PrivilegeFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

  std::string _uid;
  std::string _gid;

  void dropPrivilegesPermanently();

 private:
  void extractPrivileges();

#ifdef ARANGODB_HAVE_SETUID
  TRI_uid_t _numericUid{};
#endif
#ifdef ARANGODB_HAVE_SETGID
  TRI_gid_t _numericGid{};
#endif
};

}  // namespace arangodb
