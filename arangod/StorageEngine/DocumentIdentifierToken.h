////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STORAGE_ENGINE_DOCUMENT_IDENTIFIER_TOKEN_H
#define ARANGOD_STORAGE_ENGINE_DOCUMENT_IDENTIFIER_TOKEN_H 1

namespace arangodb {

// @brief This token is handed out by Indexes and
// is used by StorageEngines to return a document
// Only specializations of this token can be created.

struct DocumentIdentifierToken {
 public:
  // TODO Replace by Engine::InvalidToken
  constexpr DocumentIdentifierToken() : _data(0) {}

  DocumentIdentifierToken(DocumentIdentifierToken const& other) noexcept
      : _data(other._data) {}
  DocumentIdentifierToken& operator=(DocumentIdentifierToken const& other) noexcept {
    _data = other._data;
    return *this;
  }

  DocumentIdentifierToken(DocumentIdentifierToken&& other) noexcept
      : _data(other._data) {
    other._data = 0;
  }

  DocumentIdentifierToken& operator=(DocumentIdentifierToken&& other) noexcept {
    _data = other._data;
    other._data = 0;
    return *this;
  }

  ~DocumentIdentifierToken() {}

  inline bool operator==(DocumentIdentifierToken const& other) const {
    return _data == other._data;
  }

  inline bool operator==(uint64_t const& other) const { return _data == other; }

  inline bool operator!=(DocumentIdentifierToken const& other) const {
    return !(operator==(other));
  }
  inline bool operator!=(uint64_t const& other) const {
    return !(operator==(other));
  }

 protected:
  explicit DocumentIdentifierToken(uint64_t data) : _data(data) {}

 public:
  uint64_t _data;
};
}  // namespace arangodb

namespace std {
template <>
struct hash<arangodb::DocumentIdentifierToken> {
  inline size_t operator()(arangodb::DocumentIdentifierToken const& token) const noexcept {
    return token._data;
  }
};

template <>
struct equal_to<arangodb::DocumentIdentifierToken> {
  bool operator()(arangodb::DocumentIdentifierToken const& lhs,
                  arangodb::DocumentIdentifierToken const& rhs) const {
    return lhs == rhs;
  }
};

}  // namespace std

#endif
