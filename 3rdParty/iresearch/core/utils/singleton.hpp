////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_SINGLETONE_H
#define IRESEARCH_SINGLETONE_H

// internal iResearch functionality not to be exported outside main library (so/dll)
#ifdef IRESEARCH_DLL_PLUGIN
  static_assert(false, "Singleton should not be visible in plugins")
#else

#include "shared.hpp"
#include "utils/noncopyable.hpp"

namespace iresearch {

template< typename T >
class singleton: util::noncopyable {
 public:
  static T& instance() {
    static T inst;
    return inst;
  }

 protected:
  singleton() = default;
};

} // namespace iresearch {

#endif // #ifdef IRESEARCH_DLL_PLUGIN
#endif
