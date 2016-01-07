////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "DummyShell.h"

#include <iostream>

#include "Basics/tri-strings.h"

using namespace std;
using namespace triagens;
using namespace arangodb;




DummyShell::DummyShell(std::string const& history, Completer* completer)
    : ShellBase(history, completer) {}


DummyShell::~DummyShell() { close(); }


////////////////////////////////////////////////////////////////////////////////
/// @brief line editor open
////////////////////////////////////////////////////////////////////////////////

bool DummyShell::open(bool) {
  _state = STATE_OPENED;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool DummyShell::close() {
  _state = STATE_CLOSED;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void DummyShell::addHistory(const string&) {}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool DummyShell::writeHistory() { return true; }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string DummyShell::getLine(const std::string& prompt, bool& eof) {
  std::cout << prompt << flush;

  string line;
  getline(cin, line);

  if (cin.eof()) {
    eof = true;
    return "";
  }

  return line;
}


