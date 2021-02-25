////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "JavaScriptSecurityContext.h"

using namespace arangodb;

/// @brief return the context type as a string
char const* JavaScriptSecurityContext::typeName() const {
  switch (_type) {
    case Type::Restricted:
      return "restricted";
    case Type::Internal:
      return "internal";
    case Type::AdminScript:
      return "admin script";
    case Type::Query:
      return "query";
    case Type::Task:
      return "task";
    case Type::RestAction:
      return "rest action";
    case Type::RestAdminScriptAction:
      return "rest admin script action";
  }
  // should not happen
  return "unknown";
}

void JavaScriptSecurityContext::reset() {
  _canUseDatabase = false;
}

bool JavaScriptSecurityContext::canDefineHttpAction() const {
  return _type == Type::Internal;
}

bool JavaScriptSecurityContext::canReadFs() const {
  return _type == Type::Internal;
}

bool JavaScriptSecurityContext::canWriteFs() const {
  return _type == Type::Internal;
}

bool JavaScriptSecurityContext::canControlProcesses() const {
  return _type == Type::Internal || _type == Type::AdminScript || _type == Type::RestAdminScriptAction;
}

/*static*/ JavaScriptSecurityContext JavaScriptSecurityContext::createRestrictedContext() {
  JavaScriptSecurityContext context(Type::Restricted);
  context._canUseDatabase = false;
  return context;
}

/*static*/ JavaScriptSecurityContext JavaScriptSecurityContext::createInternalContext() {
  JavaScriptSecurityContext context(Type::Internal);
  context._canUseDatabase = true;
  return context;
}

/*static*/ JavaScriptSecurityContext JavaScriptSecurityContext::createAdminScriptContext() {
  JavaScriptSecurityContext context(Type::AdminScript);
  context._canUseDatabase = true;
  return context;
}

/*static*/ JavaScriptSecurityContext JavaScriptSecurityContext::createQueryContext() {
  JavaScriptSecurityContext context(Type::Query);
  context._canUseDatabase = false;
  return context;
}

/*static*/ JavaScriptSecurityContext JavaScriptSecurityContext::createTaskContext(bool allowUseDatabase) {
  JavaScriptSecurityContext context(Type::Task);
  context._canUseDatabase = allowUseDatabase;
  return context;
}

/*static*/ JavaScriptSecurityContext JavaScriptSecurityContext::createRestActionContext(bool allowUseDatabase) {
  JavaScriptSecurityContext context(Type::RestAction);
  context._canUseDatabase = allowUseDatabase;
  return context;
}

/*static*/ JavaScriptSecurityContext JavaScriptSecurityContext::createRestAdminScriptActionContext(bool allowUseDatabase) {
  JavaScriptSecurityContext context(Type::RestAdminScriptAction);
  context._canUseDatabase = allowUseDatabase;
  return context;
}
