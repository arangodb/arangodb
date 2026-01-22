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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Basics/threads-posix.h"
#include "Containers/Concurrent/shared.h"
#include "Inspection/Format.h"

namespace arangodb::basics {

struct ThreadId {
  static auto current() noexcept -> ThreadId;
  auto name() -> std::string;
  TRI_tid_t posix_id;
  pid_t kernel_id;
  bool operator==(ThreadId const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, ThreadId& x) {
  return f.object(x).fields(f.field("LWPID", x.kernel_id),
                            f.field("posix_id", x.posix_id));
}

struct ThreadInfo {
  static auto current() noexcept -> containers::SharedPtr<ThreadInfo>;
  pid_t kernel_id;
  std::string name;
  bool operator==(ThreadInfo const&) const = default;
};
template<typename Inspector>
auto inspect(Inspector& f, ThreadInfo& x) {
  return f.object(x).fields(f.field("LWPID", x.kernel_id),
                            f.field("name", x.name));
}

}  // namespace arangodb::basics

template<>
struct fmt::formatter<arangodb::basics::ThreadId>
    : arangodb::inspection::inspection_formatter {};
