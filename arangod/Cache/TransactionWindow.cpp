////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "Cache/TransactionWindow.h"

#include <stdint.h>
#include <atomic>

using namespace arangodb::cache;

TransactionWindow::TransactionWindow() : _open(0), _term(0) {}

void TransactionWindow::start() {
  if (++_open == 1) {
    _term++;
  }
}

void TransactionWindow::end() {
  if (--_open == 0) {
    _term++;
  }
}

uint64_t TransactionWindow::term() { return _term.load(); }
