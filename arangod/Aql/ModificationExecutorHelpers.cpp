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

Result ModificationExecutorHelpers::getKeyAndRevision(CollectionNameResolver const& resolver,
                                                      AqlValue const& value,
                                                      std::string& key,
                                                      std::string& rev, Revision what) {
  TRI_ASSERT(key.empty());
  TRI_ASSERT(rev.empty());
  if (value.isObject()) {
    bool mustDestroy;
    AqlValue sub = value.get(resolver, StaticStrings::KeyString, mustDestroy, false);
    AqlValueGuard guard(sub, mustDestroy);

    if (sub.isString()) {
      key.assign(sub.slice().copyString());

      if (what == Revision::Include) {
        bool mustDestroyToo;
        AqlValue subTwo =
            value.get(resolver, StaticStrings::RevString, mustDestroyToo, false);
        AqlValueGuard guard(subTwo, mustDestroyToo);
        if (subTwo.isString()) {
          rev.assign(subTwo.slice().copyString());
        } else {
          return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                        std::string{"Expected _rev as string, but got "} +
                            value.slice().typeName());
        }
      }
    } else {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                    std::string{"Expected _key as string, but got "} +
                        value.slice().typeName());
    }
  } else if (value.isString()) {
    key.assign(value.slice().copyString());
    rev.clear();
  } else {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING,
                  std::string{"Expected object or string, but got "} +
                      value.slice().typeName());
  }
  return Result{};
}

void ModificationExecutorHelpers::buildKeyDocument(VPackBuilder& builder,
                                                   std::string const& key,
                                                   std::string const& rev, Revision what) {
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(key));

  if (what == Revision::Include && !rev.empty()) {
    builder.add(StaticStrings::RevString, VPackValue(rev));
  } else {
    builder.add(StaticStrings::RevString, VPackValue(VPackValueType::Null));
  }
  builder.close();
}

bool ModificationExecutorHelpers::writeRequired(ModificationExecutorInfos& infos,
                                                VPackSlice const& doc,
                                                std::string const& key) {
  return (!infos._consultAqlWriteFilter ||
          !infos._aqlCollection->getCollection()->skipForAqlWrite(doc, key));
}

void ModificationExecutorHelpers::throwOperationResultException(
    ModificationExecutorInfos& infos, OperationResult const& result) {
  auto const& errorCounter = result.countErrorCodes;

  // Early escape if we are ignoring errors.
  if (infos._ignoreErrors == true || errorCounter.empty()) {
    return;
  }

  std::string message;

  // Find the first relevant error for which we want to throw.
  // If _ignoreDocumentNotFound is true, then this is any error other than
  // TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, otherwise it is just any error.
  //
  // Find the first error with a message and throw that
  // This mirrors previous behaviour and might not be entirely ideal.
  for (auto const p : errorCounter) {
    auto const errorCode = p.first;
    if (!(infos._ignoreDocumentNotFound && errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      // Find the first error and throw with message.
      for (auto doc : VPackArrayIterator(result.slice())) {
        if (doc.isObject() && doc.hasKey(StaticStrings::ErrorNum) &&
            doc.get(StaticStrings::ErrorNum).getInt() == errorCode) {
          VPackSlice s = doc.get(StaticStrings::ErrorMessage);
          if (s.isString()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(errorCode, s.copyString());
          }
        }
      }
      // if we did not find a message, we still throw something, because we know
      // that a relevant error has happened
      THROW_ARANGO_EXCEPTION(errorCode);
    }
  }
}

// Convert ModificationOptions to OperationOptions struct
OperationOptions ModificationExecutorHelpers::convertOptions(ModificationOptions const& in,
                                                             Variable const* outVariableNew,
                                                             Variable const* outVariableOld) {
  OperationOptions out;

  // commented out OperationOptions attributesare not provided
  // by the ModificationOptions or the information given by the
  // Variable pointer.

  // in.ignoreErrors;
  out.waitForSync = in.waitForSync;
  out.keepNull = !in.nullMeansRemove;
  out.mergeObjects = in.mergeObjects;
  // in.ignoreDocumentNotFound;
  // in.readCompleteInput;
  out.isRestore = in.useIsRestore;
  // in.consultAqlWriteFilter;
  // in.exclusive;
  out.overwrite = in.overwrite;
  out.ignoreRevs = in.ignoreRevs;

  out.returnNew = (outVariableNew != nullptr);
  out.returnOld = (outVariableOld != nullptr);
  out.silent = !(out.returnNew || out.returnOld);

  return out;
}
