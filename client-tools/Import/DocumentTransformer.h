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
/// @author Simran Spiller
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb::import {

enum class TransformAction { kEmit, kEmitMultiple, kSkip, kError };

struct TransformResult {
  TransformAction action;
  velocypack::Builder result;  // populated when action == kEmit or kEmitMultiple
  std::string error;           // populated when action == kError
};

/// @brief Abstract interface for document transformers.
/// Keeps ImportHelper free of AQL dependencies so that arangoimport_utils
/// (shared with arangosh) does not need to link arango_aql_standalone.
class IDocumentTransformer {
 public:
  virtual ~IDocumentTransformer() = default;
  virtual TransformResult transform(velocypack::Slice doc) = 0;
};

}  // namespace arangodb::import
