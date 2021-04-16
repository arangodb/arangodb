////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_META_STATIC_ASSERT_SIZE_H
#define ARANGODB_META_STATIC_ASSERT_SIZE_H 1

namespace arangodb {
namespace meta {
namespace details {

// This template function allows to statically assert the size of a type, while giving
// useful error messages about the actual sizes in question.
//
// This is for example useful to make sure methods are checked if someone adds member
// variables to classes or structs
template <typename Scrutinee, std::size_t ExpectedSize, std::size_t Size = sizeof(Scrutinee)>
void static_assert_size() {
    static_assert(ExpectedSize == Size, "Type does not have the expected size.");
}

}  // namespace details
}  // namespace meta
}  // namespace arangodb

#endif
