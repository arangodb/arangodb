////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "ModificationExecutorHelpers.h"

#include "Aql/AqlValue.h"
#include "Aql/ModificationExecutorInfos.h"
#include "Basics/Result.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <string>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

Result ModificationExecutorHelpers::getKey(CollectionNameResolver const& resolver,
                                           AqlValue const& value, std::string& key) {
  TRI_ASSERT(key.empty());

  // If `value` is a string, this is our _key entry, so we use that.
  if (value.isString()) {
    key.assign(value.slice().copyString());
    return Result{};
  }

  if (!value.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                  std::string{"Expected object or string, but got "} +
                      value.slice().typeName());
  }

  // not necessary to check if key exists in object, since AqlValue::get() will return a
  // null-result below in case key does not exist.

  // Extract key from `value`, and make sure it is a string
  bool mustDestroyKey;
  AqlValue keyEntry = value.get(resolver, StaticStrings::KeyString, mustDestroyKey, false);
  AqlValueGuard keyGuard(keyEntry, mustDestroyKey);

  if (!keyEntry.isString()) {
    return Result{TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING,
                  std::string{"Expected _key to be a string attribute in document."}};
  }

  // Key found and assigned, note rev is empty by assertion
  key.assign(keyEntry.slice().copyString());

  return Result{};
}

Result ModificationExecutorHelpers::getRevision(CollectionNameResolver const& resolver,
                                                AqlValue const& value, std::string& rev) {
  TRI_ASSERT(rev.empty());

  if (!value.isObject()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                  std::string{"Expected object, but got "} + value.slice().typeName());
  }

  if (value.hasKey(StaticStrings::RevString)) {
    bool mustDestroyRev;
    AqlValue revEntry =
        value.get(resolver, StaticStrings::RevString, mustDestroyRev, false);
    AqlValueGuard revGuard(revEntry, mustDestroyRev);

    if (!revEntry.isString()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                    std::string{"Expected _rev as string, but got "} +
                        value.slice().typeName());
    }

    // Rev found and assigned
    rev.assign(revEntry.slice().copyString());
  }  // else we leave rev empty
  return Result{};
}

Result ModificationExecutorHelpers::getKeyAndRevision(CollectionNameResolver const& resolver,
                                                      AqlValue const& value,
                                                      std::string& key, std::string& rev) {
  Result result = getKey(resolver, value, key);
  // The key can either be a string, or contained in an object
  // If it is passed in as a string, then there is no revision
  // and there is no point in extracting it further on.
  if (!result.ok() || value.isString()) {
    return result;
  }
  return getRevision(resolver, value, rev);
}

void ModificationExecutorHelpers::buildKeyDocument(VPackBuilder& builder,
                                                   std::string const& key) {
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(key));
  builder.close();
}

void ModificationExecutorHelpers::buildKeyAndRevDocument(VPackBuilder& builder,
                                                         std::string const& key,
                                                         std::string const& rev) {
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(key));

  if (rev.empty()) {
    // Necessary to sometimes remove _rev entries
    builder.add(StaticStrings::RevString, VPackValue(VPackValueType::Null));
  } else {
    builder.add(StaticStrings::RevString, VPackValue(rev));
  }
  builder.close();
}

bool ModificationExecutorHelpers::writeRequired(ModificationExecutorInfos const& infos,
                                                VPackSlice const& doc,
                                                std::string const& key) {
  return (!infos._consultAqlWriteFilter ||
          !infos._aqlCollection->getCollection()->skipForAqlWrite(doc, key));
}

void ModificationExecutorHelpers::throwOperationResultException(
    ModificationExecutorInfos const& infos, OperationResult const& operationResult) {

  // A "higher level error" happened (such as the transaction being aborted,
  // replication being refused, etc ), and we do not have errorCounter or
  // similar so we throw.
  if (!operationResult.ok()) {
    // inside OperationResult hides a small result.
    THROW_ARANGO_EXCEPTION(operationResult.result);
  }

  auto const& errorCounter = operationResult.countErrorCodes;

  // Early escape if we are ignoring errors.
  if (infos._ignoreErrors == true || errorCounter.empty()) {
    return;
  }

  // Find the first relevant error for which we want to throw.
  // If _ignoreDocumentNotFound is true, then this is any error other than
  // TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, otherwise it is just any error.
  //
  // Find the first error with a message and throw that
  // This mirrors previous behaviour and might not be entirely ideal.
  for (auto const& p : errorCounter) {
    auto const errorCode = p.first;
    if (!(infos._ignoreDocumentNotFound && errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      // Find the first error and throw with message.
      for (auto doc : VPackArrayIterator(operationResult.slice())) {
        if (doc.isObject() && doc.hasKey(StaticStrings::ErrorNum) &&
            doc.get(StaticStrings::ErrorNum).getInt() == errorCode) {
          VPackSlice s = doc.get(StaticStrings::ErrorMessage);
          if (s.isString()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, s.copyString());
          }
        }
      }
      // if we did not find a message, we still throw something, because we
      // know that a relevant error has happened
      THROW_ARANGO_EXCEPTION(errorCode);
    }
  }
}

// Convert ModificationOptions to OperationOptions struct
OperationOptions ModificationExecutorHelpers::convertOptions(ModificationOptions const& in,
                                                             Variable const* outVariableNew,
                                                             Variable const* outVariableOld) {
  OperationOptions out;

  // commented out OperationOptions attributes are not provided
  // by the ModificationOptions or the information given by the
  // Variable pointer.

  // in.ignoreErrors;
  out.waitForSync = in.waitForSync;
  out.validate = in.validate;
  out.keepNull = in.keepNull;
  out.mergeObjects = in.mergeObjects;
  // in.ignoreDocumentNotFound;
  // in.readCompleteInput;
  out.isRestore = in.isRestore;
  // in.consultAqlWriteFilter;
  // in.exclusive;
  out.overwriteMode = in.overwriteMode;
  out.ignoreRevs = in.ignoreRevs;

  out.returnNew = (outVariableNew != nullptr);
  out.returnOld = (outVariableOld != nullptr);
  out.silent = !(out.returnNew || out.returnOld);

  return out;
}

AqlValue ModificationExecutorHelpers::getDocumentOrNull(VPackSlice const& elm,
                                                        std::string const& key) {
  VPackSlice s = elm.get(key);
  if (!s.isNone()) {
    return AqlValue{s};
  }
  return AqlValue(AqlValueHintNull());
}
