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

enum class Revision { Include, Exclude };

Result getKeyAndRevision(CollectionNameResolver const& resolver,
                         AqlValue const& value, std::string& key,
                         std::string& rev, Revision what = Revision::Include);
Result buildKeyDocument(VPackBuilder& builder, std::string const& key,
                        std::string const& rev, Revision what = Revision::Include);
bool writeRequired(ModificationExecutorInfos& infos, VPackSlice const& doc,
                   std::string const& key);
void throwOperationResultException(ModificationExecutorInfos& infos,
                                   OperationResult const& result);

OperationOptions convertOptions(ModificationOptions const& in, Variable const* outVariableNew,
                                Variable const* outVariableOld);

}  // namespace ModificationExecutorHelpers
}  // namespace aql
}  // namespace arangodb

#endif
