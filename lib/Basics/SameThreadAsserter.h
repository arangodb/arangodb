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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_SAME_THREAD_ASSERTER_H
#define ARANGODB_BASICS_SAME_THREAD_ASSERTER_H 1

#include "Basics/Common.h"
#include "Basics/Thread.h"

namespace arangodb {

class SameThreadAsserter {
 public:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

  SameThreadAsserter() : _startingThreadId(currentThreadId()) {}

  ~SameThreadAsserter() { TRI_ASSERT(_startingThreadId == currentThreadId()); }

 private:
  TRI_tid_t currentThreadId() const { return Thread::currentThreadId(); }

 private:
  TRI_tid_t const _startingThreadId;

#else

  SameThreadAsserter() {}
  ~SameThreadAsserter() {}

#endif
};

}  // namespace arangodb

#endif
