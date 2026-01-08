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

#include <optional>
#include <string>

namespace arangodb {

// Non-Owning DocumentId
struct DocumentIdView {
  static auto fromStringView(std::string_view id)
      -> std::optional<DocumentIdView> {
    auto sep = id.find('/');
    if (sep == std::string::npos) {
      return std::nullopt;
    } else {
      return DocumentIdView(std::move(id), sep);
    }
  };

  explicit DocumentIdView(std::string_view id, std::string::size_type sep)
      : _id{std::move(id)}, _sep(std::move(sep)) {}

  DocumentIdView(DocumentIdView const&) = default;
  DocumentIdView(DocumentIdView&&) = default;
  DocumentIdView& operator=(DocumentIdView const&) = default;
  DocumentIdView& operator=(DocumentIdView&&) = default;

  auto operator<=>(DocumentIdView const& other) const -> std::strong_ordering {
    return _id <=> other._id;
  }
  auto operator==(DocumentIdView const& other) const {
    return _id == other._id;
  }

  // TODO: This is not consistent with std; std::string_view::substr
  // returns a string_view; should we allow constructing a DocumentId from a
  // DocumentIdView to explicitly copy? Provide a copy() function?
  auto collectionName() const -> std::string {
    return std::string{_id.substr(0, _sep)};
  }
  auto collectionNameView() const -> std::string_view {
    return _id.substr(0, _sep);
  }

  auto key() const -> std::string {
    return std::string{_id.substr(_sep + 1, _id.size())};
  }
  auto keyView() const -> std::string_view {
    return _id.substr(_sep + 1, _id.size());
  }

  auto id() const -> std::string { return std::string{_id}; }

  auto idView() const -> std::string_view { return _id; }

 private:
  std::string_view _id;
  std::string::size_type _sep;
};
}  // namespace arangodb

namespace std {
template<>
struct hash<arangodb::DocumentIdView> {
  inline size_t operator()(
      arangodb::DocumentIdView const& value) const noexcept {
    return std::hash<std::string_view>{}(value.idView());
  }
};

}  // namespace std
