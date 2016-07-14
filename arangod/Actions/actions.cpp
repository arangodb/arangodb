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

#include "actions.h"

#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "Rest/GeneralRequest.h"

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief actions
////////////////////////////////////////////////////////////////////////////////

static std::map<std::string, TRI_action_t*> Actions;

////////////////////////////////////////////////////////////////////////////////
/// @brief prefix actions
////////////////////////////////////////////////////////////////////////////////

static std::map<std::string, TRI_action_t*> PrefixActions;

////////////////////////////////////////////////////////////////////////////////
/// @brief actions lock
////////////////////////////////////////////////////////////////////////////////

static ReadWriteLock ActionsLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t* TRI_DefineActionVocBase(std::string const& name,
                                      TRI_action_t* action) {
  WRITE_LOCKER(writeLocker, ActionsLock);

  std::string url = name;

  while (!url.empty() && url[0] == '/') {
    url = url.substr(1);
  }

  action->_url = url;
  action->_urlParts = StringUtils::split(url, "/").size();

  // create a new action and store the callback function
  if (action->_isPrefix) {
    if (PrefixActions.find(url) == PrefixActions.end()) {
      PrefixActions[url] = action;
    } else {
      TRI_action_t* oldAction = PrefixActions[url];

      if (oldAction->_type != action->_type) {
        LOG(ERR) << "trying to define two incompatible actions of type '" << oldAction->_type << "' and '" << action->_type << "' for prefix url '" << action->_url << "'";

        delete oldAction;
      } else {
        delete action;
        action = oldAction;
      }
    }
  } else {
    if (Actions.find(url) == Actions.end()) {
      Actions[url] = action;
    } else {
      TRI_action_t* oldAction = Actions[url];

      if (oldAction->_type != action->_type) {
        LOG(ERR) << "trying to define two incompatible actions of type '" << oldAction->_type << "' and type '" << action->_type << "' for url '" << action->_url << "'";

        delete oldAction;
      } else {
        delete action;
        action = oldAction;
      }
    }
  }

  // some debug output
  LOG(DEBUG) << "created " << action->_type << " " << (action->_isPrefix ? "prefix " : "") << " '" << url << "'";

  // return old or new action description
  return action;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an action
////////////////////////////////////////////////////////////////////////////////

TRI_action_t* TRI_LookupActionVocBase(arangodb::GeneralRequest* request) {
  // check if we know a callback
  std::vector<std::string> suffix = request->suffix();

  // find a direct match
  std::string name = StringUtils::join(suffix, '/');

  READ_LOCKER(readLocker, ActionsLock);
  std::map<std::string, TRI_action_t*>::iterator i = Actions.find(name);

  if (i != Actions.end()) {
    return i->second;
  }

  // find longest prefix match
  while (true) {
    name = StringUtils::join(suffix, '/');
    i = PrefixActions.find(name);

    if (i != PrefixActions.end()) {
      return i->second;
    }

    if (suffix.empty()) {
      break;
    }

    suffix.pop_back();
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes all defined actions
////////////////////////////////////////////////////////////////////////////////

void TRI_CleanupActions() {
  for (auto& it : Actions) {
    delete it.second;
  }
  Actions.clear();

  for (auto& it : PrefixActions) {
    delete it.second;
  }
  PrefixActions.clear();
}
