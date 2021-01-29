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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "attribute_store.hpp"

namespace {

const irs::attribute_store EMPTY_ATTRIBUTE_STORE(0);

}

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                                   attribute_store
// -----------------------------------------------------------------------------

attribute_store::attribute_store(size_t /*reserve = 0*/) {
}

/*static*/ const attribute_store& attribute_store::empty_instance() {  
  return EMPTY_ATTRIBUTE_STORE;
}

}
