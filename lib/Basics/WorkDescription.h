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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_WORK_DESCRIPTION_H
#define ARANGODB_BASICS_WORK_DESCRIPTION_H 1

#include "Common.h"

struct TRI_vocbase_t;

namespace arangodb {
class Thread;

namespace rest {
class RestHandler;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                          WorkType
// -----------------------------------------------------------------------------

enum class WorkType { THREAD, HANDLER, AQL_STRING, AQL_ID, CUSTOM };

// -----------------------------------------------------------------------------
// --SECTION--                                                       WorkContext
// -----------------------------------------------------------------------------

struct WorkContext {
  WorkContext() {}
  WorkContext(std::string const& user) : _user(user) {}
  WorkContext(std::string const& user, std::string const& database)
      : _user(user), _database(database) {}

  std::string _user;
  std::string _database;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   WorkDescription
// -----------------------------------------------------------------------------

struct WorkDescription {
  WorkDescription(WorkType type, WorkDescription* prev)
      : _type(type), _id(0), _prev(prev) {
    if (prev != nullptr) {
      _context = prev->_context;
    }
  }

  WorkDescription(WorkType type, std::shared_ptr<WorkContext> context, WorkDescription* prev)
      : _type(type), _id(0), _context(context), _prev(prev) {}

  WorkType _type;
  uint64_t _id;

  std::shared_ptr<WorkContext> _context;
  std::atomic<WorkDescription*> _prev;

  union Data {
    Data() {}
    ~Data() {}

    // THREAD
    struct ThreadMember {
      ThreadMember(Thread* thread, bool canceled)
          : _thread(thread), _canceled(canceled) {}

      Thread* _thread;
      std::atomic<bool> _canceled;
    } _thread;

    // HANDLER
    struct HandlerMember {
      std::shared_ptr<rest::RestHandler> _handler;
      std::atomic<bool> _canceled;
    } _handler;

    // AQL_STRING, AQL_ID
    struct AqlStringMember {
      TRI_vocbase_t* _vocbase;
      uint64_t _id;
      char _text[256];
      std::atomic<bool> _canceled;
    } _aql;

    // CUSTOM
    struct CustomMember {
      char _type[16];
      char _text[256];
    } _custom;
  } _data;
};
}  // namespace arangodb

#endif
