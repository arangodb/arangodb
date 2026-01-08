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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <compare>
#include <string>
#include <optional>

namespace arangodb {

struct DocumentId {
  static auto fromString(std::string id) -> std::optional<DocumentId> {
    auto sep = id.find('/');
    if (sep == std::string::npos) {
      return std::nullopt;
    } else {
      return DocumentId(std::move(id), sep);
    }
  }

  explicit DocumentId(std::string id, std::string::size_type sep)
      : _sep(sep), _id(std::move(id)) {}

  DocumentId(DocumentId const&) = default;
  DocumentId(DocumentId&&) = default;
  DocumentId& operator=(DocumentId const&) = default;
  DocumentId& operator=(DocumentId&&) = default;

  auto operator<=>(DocumentId const& other) const -> std::strong_ordering {
    return _id <=> other._id;
  }
  auto operator==(DocumentId const& other) const { return _id == other._id; }

  auto collectionName() const -> std::string { return _id.substr(0, _sep); }
  auto collectionNameView() const -> std::string_view {
    return std::string_view{_id}.substr(0, _sep);
  }

  auto key() const -> std::string { return _id.substr(_sep + 1, _id.size()); }
  auto keyView() const -> std::string_view {
    return std::string_view{_id}.substr(_sep + 1, _id.size());
  }

 private:
  std::string::size_type _sep;
  std::string _id;
};

}  // namespace arangodb
