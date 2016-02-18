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

#ifndef ARANGOD_ACTIONS_ACTIONS_H
#define ARANGOD_ACTIONS_ACTIONS_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace rest {
class HttpResponse;
class HttpRequest;
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief action result
////////////////////////////////////////////////////////////////////////////////

class TRI_action_result_t {
 public:
  TRI_action_result_t() : isValid(false), canceled(false), response(nullptr) {}

  // Please be careful here: In the beginning we had "bool requeue" after
  // the response pointer in this struct. However, this triggered a nasty
  // compiler bug in Visual Studio Express 2013 which lead to the fact
  // that sometimes requeue was involuntarily flipped to "true" during
  // a return of a TRI_action_result_t from a function call.
  // In this order it seems to work.
  // Details: v8-actions.cpp: v8_action_t::execute returns a TRI_action_result_t
  // to RestActionHandler::executeAction and suddenly requeue is true.

  bool isValid;
  bool canceled;

  arangodb::rest::HttpResponse* response;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief action descriptor
////////////////////////////////////////////////////////////////////////////////

class TRI_action_t {
 public:
  TRI_action_t()
      : _type(),
        _url(),
        _urlParts(0),
        _isPrefix(false),
        _allowUseDatabase(false) {}

  virtual ~TRI_action_t() {}

  virtual TRI_action_result_t execute(TRI_vocbase_t*,
                                      arangodb::rest::HttpRequest*,
                                      arangodb::Mutex* dataLock,
                                      void** data) = 0;

  virtual bool cancel(arangodb::Mutex* dataLock, void** data) = 0;

  std::string _type;
  std::string _url;

  size_t _urlParts;

  bool _isPrefix;
  bool _allowUseDatabase;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t* TRI_DefineActionVocBase(std::string const& name,
                                      TRI_action_t* action);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t* TRI_LookupActionVocBase(arangodb::rest::HttpRequest* request);

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes all defined actions
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupActions();

#endif
