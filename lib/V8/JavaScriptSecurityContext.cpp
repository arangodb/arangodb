////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "JavaScriptSecurityContext.h"

using namespace arangodb;

/// @brief return the context type as a string
std::string_view JavaScriptSecurityContext::typeName() const noexcept {
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
      return "REST action";
    case Type::RestAdminScriptAction:
      return "REST admin script action";
  }
  // should not happen
  return "unknown";
}

void JavaScriptSecurityContext::reset() noexcept { _canUseDatabase = false; }

bool JavaScriptSecurityContext::canDefineHttpAction() const noexcept {
  return _type == Type::Internal;
}

bool JavaScriptSecurityContext::canReadFs() const noexcept {
  return _type == Type::Internal;
}

bool JavaScriptSecurityContext::canWriteFs() const noexcept {
  return _type == Type::Internal;
}

bool JavaScriptSecurityContext::canControlProcesses() const noexcept {
  return _type == Type::Internal || _type == Type::AdminScript ||
         _type == Type::RestAdminScriptAction;
}

/*static*/ JavaScriptSecurityContext
JavaScriptSecurityContext::createRestrictedContext() noexcept {
  JavaScriptSecurityContext context(Type::Restricted);
  context._canUseDatabase = false;
  return context;
}

/*static*/ JavaScriptSecurityContext
JavaScriptSecurityContext::createInternalContext() noexcept {
  JavaScriptSecurityContext context(Type::Internal);
  context._canUseDatabase = true;
  return context;
}

/*static*/ JavaScriptSecurityContext
JavaScriptSecurityContext::createAdminScriptContext() noexcept {
  JavaScriptSecurityContext context(Type::AdminScript);
  context._canUseDatabase = true;
  return context;
}

/*static*/ JavaScriptSecurityContext
JavaScriptSecurityContext::createQueryContext() noexcept {
  JavaScriptSecurityContext context(Type::Query);
  context._canUseDatabase = false;
  return context;
}

/*static*/ JavaScriptSecurityContext
JavaScriptSecurityContext::createTaskContext(bool allowUseDatabase) noexcept {
  JavaScriptSecurityContext context(Type::Task);
  context._canUseDatabase = allowUseDatabase;
  return context;
}

/*static*/ JavaScriptSecurityContext
JavaScriptSecurityContext::createRestActionContext(
    bool allowUseDatabase) noexcept {
  JavaScriptSecurityContext context(Type::RestAction);
  context._canUseDatabase = allowUseDatabase;
  return context;
}

/*static*/ JavaScriptSecurityContext
JavaScriptSecurityContext::createRestAdminScriptActionContext(
    bool allowUseDatabase) noexcept {
  JavaScriptSecurityContext context(Type::RestAdminScriptAction);
  context._canUseDatabase = allowUseDatabase;
  return context;
}
