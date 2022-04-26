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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Guarded.h"

#include "Basics/UnshackledMutex.h"
#include "Basics/UnshackledConditionVariable.h"

using namespace arangodb;

template<typename Lock, typename ConditionVariable>
std::enable_if_t<
    std::is_same_v<ConditionVariable, std::condition_variable> &&
        std::is_same_v<Lock, std::unique_lock<std::mutex>> ||
    std::is_same_v<ConditionVariable, basics::UnshackledConditionVariable> &&
        std::is_same_v<Lock, std::unique_lock<basics::UnshackledMutex>>>
detail::wait(Lock& lock, ConditionVariable& cv) {
  return cv.wait(lock);
}

template void detail::wait(std::unique_lock<std::mutex>&,
                           std::condition_variable&);
template void detail::wait(std::unique_lock<basics::UnshackledMutex>&,
                           basics::UnshackledConditionVariable&);
