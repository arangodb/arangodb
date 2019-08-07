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

#ifndef ARANGODB_V8_V8LINE_EDITOR_H
#define ARANGODB_V8_V8LINE_EDITOR_H 1

#include <atomic>
#include <string>

#include <v8.h>

#include "Utilities/LineEditor.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief V8LineEditor
////////////////////////////////////////////////////////////////////////////////

class V8LineEditor : public LineEditor {
  V8LineEditor(LineEditor const&) = delete;
  V8LineEditor& operator=(LineEditor const&) = delete;

 public:
  V8LineEditor(v8::Isolate*, v8::Handle<v8::Context>, std::string const& history);

  ~V8LineEditor();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the editor's isolate
  //////////////////////////////////////////////////////////////////////////////

  v8::Isolate* isolate() const { return _isolate; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief test whether we are executing a command
  //////////////////////////////////////////////////////////////////////////////

  bool isExecutingCommand() { return _executingCommand.load(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief note whether we are executing a command
  //////////////////////////////////////////////////////////////////////////////

  void setExecutingCommand(bool value) { _executingCommand.store(value); }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief isolate
  //////////////////////////////////////////////////////////////////////////////

  v8::Isolate* _isolate;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief context
  //////////////////////////////////////////////////////////////////////////////

  v8::Handle<v8::Context> _context;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not there is a command currently executing
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<bool> _executingCommand;
};
}  // namespace arangodb

#endif
