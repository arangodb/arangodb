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

#ifndef ARANGOD_AQL_MODIFICATION_EXECUTOR_HELPERS_H
#define ARANGOD_AQL_MODIFICATION_EXECUTOR_HELPERS_H

#include "Aql/AqlValue.h"
#include "Aql/ModificationOptions.h"
#include "Basics/Result.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <string>

namespace arangodb {
namespace aql {

struct ModificationExecutorInfos;

namespace ModificationExecutorHelpers {

// Extracts key from input AqlValue value.
//
// * if value is a string, this string is assigned to key
// * if value is an object, we extract the entry _key into key if it is a string,
//   or signal an error otherwise.
// * if value is anything else, we return an error.
Result getKey(CollectionNameResolver const& resolver, AqlValue const& value,
              std::string& key);

// Extracts rev from input AqlValue value.
//
// * value has to be an objectis a string, this string is assigned to key
// * if value is an object, we extract the entry _key into key if it is a string,
//   or signal an error otherwise.
// * if value is anything else, we return an error.
Result getRevision(CollectionNameResolver const& resolver,
                   AqlValue const& value, std::string& key);

Result getKeyAndRevision(CollectionNameResolver const& resolver,
                         AqlValue const& value, std::string& key, std::string& rev);

// Builds an object "{ _key: key }"
void buildKeyDocument(VPackBuilder& builder, std::string const& key);

// Builds an object "{ _key: key, _rev: rev }" if rev is not empty, otherwise
// "{ _key: key, _rev: null }"
void buildKeyAndRevDocument(VPackBuilder& builder, std::string const& key,
                            std::string const& rev);

// Establishes whether a write is necessary. This is only relevant for
// SmartGraphs in the Enterprise Edition. Refer to skipForAqlWrite in
// Enterprise Edition
bool writeRequired(ModificationExecutorInfos const& infos,
                   VPackSlice const& doc, std::string const& key);

// Throws an exception if a transaction resulted in an error, and errors
// are not ignored.
// This function includes special handling for ignoreDocumentNotFound cases,
// which are needed in the cluster where a document not found error can happen
// but not be fatal.
void throwOperationResultException(ModificationExecutorInfos const& infos,
                                   OperationResult const& operationResult);

// Converts ModificationOptions to OperationOptions
OperationOptions convertOptions(ModificationOptions const& in, Variable const* outVariableNew,
                                Variable const* outVariableOld);

AqlValue getDocumentOrNull(VPackSlice const& elm, std::string const& key);

}  // namespace ModificationExecutorHelpers
}  // namespace aql
}  // namespace arangodb

#endif
