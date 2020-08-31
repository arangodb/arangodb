////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/DataProtector.h"

// TODO: Make this a template again once everybody has gcc >= 4.9.2
// template<int Nr>
// thread_local int arangodb::basics::DataProtector<Nr>::_mySlot = -1;
thread_local int arangodb::basics::DataProtector::_mySlot = -1;

// TODO: Make this a template again once everybody has gcc >= 4.9.2
// template<int Nr>
// std::atomic<int> arangodb::basics::DataProtector<Nr>::_last(0);
std::atomic<int> arangodb::basics::DataProtector::_last(0);

/// We need an explicit template initialization for each value of the
/// template parameter, such that the compiler can allocate a thread local
/// variable for each of them.

// TODO: Reactivate this template instantiation once everybody has gcc >= 4.9.2
// template class arangodb::basics::DataProtector<64>;

int arangodb::basics::DataProtector::getMyId() {
  int id = _mySlot;
  if (id >= 0) {
    return id;
  }
  while (true) {
    int newId = _last + 1;
    if (newId >= DATA_PROTECTOR_MULTIPLICITY) {
      newId = 0;
    }
    if (_last.compare_exchange_strong(id, newId)) {
      _mySlot = newId;
      return newId;
    }
  }
}
