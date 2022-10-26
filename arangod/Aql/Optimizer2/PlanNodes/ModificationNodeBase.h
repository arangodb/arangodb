////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Optimizer2/PlanNodes/BaseNode.h"
#include "Aql/Optimizer2/PlanNodes/CollectionAcessingNode.h"

#include "Aql/Optimizer2/PlanNodeTypes/Indexes.h"
#include "Aql/Optimizer2/PlanNodeTypes/Variable.h"
#include "Aql/Optimizer2/PlanNodeTypes/Satellite.h"

#include "Aql/Optimizer2/Types/Types.h"

namespace arangodb::aql::optimizer2::nodes {

// TODO: If this struct will be re-used somewhere else, lets move it into it's
// own file.
struct OperationOptions {
  bool waitForSync;
  bool skipDocumentValidation;
  bool keepNull;
  bool mergeObjects;
  bool ignoreRevs;
  bool isRestore;
  std::optional<bool> overwriteMode;
  bool ignoreErrors;
  bool ignoreDocumentNotFound;
  bool consultAqlWriteFilter;
  bool exclusive;

  bool operator==(OperationOptions const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, OperationOptions& v) {
  return f.object(v).fields(
      f.field("waitForSync", v.waitForSync),
      f.field("skipDocumentValidation", v.skipDocumentValidation),
      f.field("keepNull", v.keepNull), f.field("mergeObjects", v.mergeObjects),
      f.field("ignoreRevs", v.ignoreRevs), f.field("isRestore", v.isRestore),
      f.field("overwriteMode", v.overwriteMode),
      f.field("ignoreErrors", v.ignoreErrors),
      f.field("ignoreDocumentNotFound", v.ignoreDocumentNotFound),
      f.field("consultAqlWriteFilter", v.consultAqlWriteFilter),
      f.field("exclusive", v.exclusive));
}

struct ModificationOptions : OperationOptions {
  std::optional<bool> ignoreFlags;
  bool ignoreDocumentNotFound;
  // "readCompleteInput" was removed in 3.9. We'll leave it here only to be
  // downwards-compatible. TODO: remove attribute in 3.10
  bool readCompleteInput = false;
  bool consultAqlWriteFilter;
  bool exclusive;

  bool operator==(ModificationOptions const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, ModificationOptions& v) {
  return f.object(v).fields(
      f.embedFields(static_cast<optimizer2::nodes::OperationOptions&>(v)),
      f.field("ignoreFlags", v.ignoreFlags),
      f.field("ignoreDocumentNotFound", v.ignoreDocumentNotFound),
      f.field("readCompleteInput", v.readCompleteInput).fallback(false),
      f.field("consultAqlWriteFilter", v.consultAqlWriteFilter),
      f.field("exclusive", v.exclusive));
}

struct ModificationNodeBase : BaseNode,
                              CollectionAccessingNode,
                              optimizer2::types::Satellite {
  ModificationOptions modificationFlags;
  std::vector<optimizer2::types::IndexHandle> indexes;
  std::optional<optimizer2::types::Variable> outVariableOld;
  std::optional<optimizer2::types::Variable> outVariableNew;
  bool countStats;
  bool producesResults;

  bool operator==(ModificationNodeBase const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, ModificationNodeBase& v) {
  return f.object(v).fields(
      f.embedFields(static_cast<optimizer2::nodes::BaseNode&>(v)),
      f.embedFields(
          static_cast<optimizer2::nodes::CollectionAccessingNode&>(v)),
      f.embedFields(static_cast<optimizer2::types::Satellite&>(v)),
      f.field("indexes", v.indexes),
      f.field("modificationFlags", v.modificationFlags),
      f.field("outVariableOld", v.outVariableOld),
      f.field("outVariableNew", v.outVariableNew),
      f.field("countStats", v.countStats),
      f.field("producesResults", v.producesResults));
}

}  // namespace arangodb::aql::optimizer2::nodes
