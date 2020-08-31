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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "actions.h"

#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/GeneralRequest.h"

using namespace arangodb::basics;

/// @brief actions
static std::unordered_map<std::string, std::shared_ptr<TRI_action_t>> Actions;

/// @brief prefix actions
static std::unordered_map<std::string, std::shared_ptr<TRI_action_t>> PrefixActions;

/// @brief actions lock
static ReadWriteLock ActionsLock;

/// @brief actions of this type are executed directly. nothing to do here
TRI_action_result_t TRI_fake_action_t::execute(TRI_vocbase_t*,
                                               arangodb::GeneralRequest*,
                                               arangodb::GeneralResponse*,
                                               arangodb::Mutex*, void**) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "TRI_fake_action_t::execute must never be called");
}

/// @brief defines an action
std::shared_ptr<TRI_action_t> TRI_DefineActionVocBase(std::string const& name,
                                                      std::shared_ptr<TRI_action_t> action) {
  std::string url = name;

  // strip leading slash
  while (!url.empty() && url[0] == '/') {
    url = url.substr(1);
  }

  action->setUrl(url, StringUtils::split(url, '/').size());

  std::unordered_map<std::string, std::shared_ptr<TRI_action_t>>* which;

  if (action->_isPrefix) {
    which = &PrefixActions;
  } else {
    which = &Actions;
  }

  WRITE_LOCKER(writeLocker, ActionsLock);

  // create a new action and store the callback function
  auto it = which->try_emplace(url, action);

  if (!it.second) {
    return (*(it.first)).second;
  }

  // some debug output
  LOG_TOPIC("93939", DEBUG, arangodb::Logger::FIXME)
      << "created JavaScript " << (action->_isPrefix ? "prefix " : "")
      << "action '" << url << "'";
  return action;
}

/// @brief looks up an action
std::shared_ptr<TRI_action_t> TRI_LookupActionVocBase(arangodb::GeneralRequest* request) {
  // check if we know a callback
  std::vector<std::string> suffixes = request->decodedSuffixes();

  // find a direct match
  std::string name = StringUtils::join(suffixes, '/');

  READ_LOCKER(readLocker, ActionsLock);
  auto it = Actions.find(name);

  if (it != Actions.end()) {
    return (*it).second;
  }

  // find longest prefix match
  while (true) {
    it = PrefixActions.find(name);

    if (it != PrefixActions.end()) {
      // found a prefix match
      return (*it).second;
    }

    // no match. no strip last suffix and try again

    if (suffixes.empty()) {
      // no further suffix left to strip
      readLocker.unlock();
      break;
    }

    auto const& suffix = suffixes.back();
    size_t suffixLength = suffix.size();
    suffixes.pop_back();

    TRI_ASSERT(name.size() >= suffixLength);
    name.resize(name.size() - suffixLength);
    // skip over '/' char at the end
    if (!name.empty() && name.back() == '/') {
      name.resize(name.size() - 1);
    }
  }

  return nullptr;
}

/// @brief deletes all defined actions
void TRI_CleanupActions() {
  WRITE_LOCKER(writeLocker, ActionsLock);

  Actions.clear();
  PrefixActions.clear();
}

void TRI_VisitActions(std::function<void(TRI_action_t*)> const& visitor) {
  READ_LOCKER(writeLocker, ActionsLock);

  for (auto& it : Actions) {
    visitor(it.second.get());
  }

  for (auto& it : PrefixActions) {
    visitor(it.second.get());
  }
}
