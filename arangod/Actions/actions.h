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
class GeneralRequest;
class GeneralResponse;
}

/// @brief action result
class TRI_action_result_t {
 public:
  TRI_action_result_t() : isValid(false), canceled(false) {}

  bool isValid;
  bool canceled;
};

/// @brief action descriptor
class TRI_action_t {
 public:
  TRI_action_t()
      : _urlParts(0),
        _isPrefix(false),
        _allowUseDatabase(false) {}

  virtual ~TRI_action_t() {}

  virtual void visit(void*) = 0;

  virtual TRI_action_result_t execute(TRI_vocbase_t*,
                                      arangodb::GeneralRequest*,
                                      arangodb::GeneralResponse*,
                                      arangodb::Mutex* dataLock,
                                      void** data) = 0;

  virtual bool cancel(arangodb::Mutex* dataLock, void** data) = 0;

  std::string _url;

  size_t _urlParts;

  bool _isPrefix;
  bool _allowUseDatabase;
};

/// @brief defines an action
std::shared_ptr<TRI_action_t> TRI_DefineActionVocBase(std::string const& name,
                                                      std::shared_ptr<TRI_action_t> action);

/// @brief looks up an action
std::shared_ptr<TRI_action_t> TRI_LookupActionVocBase(arangodb::GeneralRequest* request);

/// @brief deletes all defined actions
void TRI_CleanupActions();

/// @brief visit all actions
void TRI_VisitActions(std::function<void(TRI_action_t*)> const& visitor);

#endif
