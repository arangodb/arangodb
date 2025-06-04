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
#include "thread.h"

#include "Basics/Thread.h"

using namespace arangodb::basics;

auto ThreadId::current() noexcept -> ThreadId {
  return ThreadId{.posix_id = arangodb::Thread::currentThreadId(),
                  .kernel_id = arangodb::Thread::currentKernelThreadId()};
}
auto ThreadId::name() -> std::string {
  return std::string{ThreadNameFetcher{posix_id}.get()};
}

auto ThreadInfo::current() noexcept -> containers::SharedPtr<ThreadInfo> {
  struct Guard {
    containers::SharedPtr<ThreadInfo> _info = containers::SharedPtr<ThreadInfo>(
        arangodb::Thread::currentKernelThreadId(),
        std::string{arangodb::ThreadNameFetcher{}.get()});
  };
  static thread_local auto guard = Guard{};
  return guard._info;
}
