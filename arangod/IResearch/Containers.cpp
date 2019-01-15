//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "utils/thread_utils.hpp"

#include "Containers.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

void ResourceMutex::reset() {
  if (get()) {
    irs::async_utils::read_write_mutex::write_mutex mutex(_mutex);
    SCOPED_LOCK(mutex);
    _resource.store(nullptr);
  }
}

NS_END      // iresearch
    NS_END  // arangodb

    // -----------------------------------------------------------------------------
    // --SECTION-- END-OF-FILE
    // -----------------------------------------------------------------------------