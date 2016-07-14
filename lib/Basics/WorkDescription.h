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

#ifndef ARANGODB_BASICS_WORK_DESCRIPTION_H
#define ARANGODB_BASICS_WORK_DESCRIPTION_H 1

#include "Common.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace rest {
class RestHandler;
}

class Thread;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of the current work
////////////////////////////////////////////////////////////////////////////////

enum class WorkType { THREAD, HANDLER, AQL_STRING, AQL_ID, CUSTOM };

////////////////////////////////////////////////////////////////////////////////
/// @brief description of the current work
////////////////////////////////////////////////////////////////////////////////

struct WorkDescription {
  WorkDescription(WorkType, WorkDescription*);

  WorkType _type;
  std::atomic<WorkDescription*> _prev;
  uint64_t _id;
  TRI_vocbase_t* _vocbase;

  bool _destroy;
  std::atomic<bool> _canceled;

  union identifer {
    char _customType[16];
    uint64_t _id;
  } _identifier;

  union data {
    data() {}
    ~data() {}

    char text[256];
    Thread* thread;
    rest::RestHandler* handler;
  } _data;
};
}

#endif
