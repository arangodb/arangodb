////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IResearch/IResearchDocument.h"
#include <index/iterators.hpp>
#include <index/index_reader.hpp>
#include <analysis/token_attributes.hpp>

namespace arangodb::iresearch {

extern const irs::payload NoPayload;

[[maybe_unused]] inline irs::doc_iterator::ptr pkColumn(
    irs::SubReader const& segment) {
  auto const* reader = segment.column(DocumentPrimaryKey::PK());

  return reader ? reader->iterator(irs::ColumnHint::kNormal) : nullptr;
}

[[maybe_unused]] inline irs::doc_iterator::ptr sortColumn(
    irs::SubReader const& segment) {
  auto const* reader = segment.sort();

  return reader ? reader->iterator(irs::ColumnHint::kNormal) : nullptr;
}
}  // namespace arangodb::iresearch
