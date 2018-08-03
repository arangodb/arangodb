////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

// write proper gurads
#pragma once

// usage

// namespace tag {
// struct required;
// struct optional;
// struct deprecated;
// }
//
// AR_DEFINE_KEYWORD_TYPE(DeprecatedType, tag::deprecated,   AttributeSet)
//
// namespace {
// AR_DEFINE_KEYWORD(_required, tag::required, AttributeSet);
// AR_DEFINE_KEYWORD(_optional, tag::optional, AttributeSet);
// AR_DEFINE_KEYWORD(_deprecated, tag::optional, AttributeSet);
// }
//
// Later in the code you can use the fresh created keyword variables
// to creates tagged_arguments by assining a vaule to them. The value
// you assigned will be stored as value member in the tagged_argument.
//
// _required = AttributeSet({"something something"}) -> tagedArgument<tag::required, AttributeSet> { AttributeSet{"something something"}}
//

namespace arangodb {
  // The following stuff is for named parameters:
template <typename Tag, typename Type>
struct tagged_argument {
  Type const& value; // assignment to const& survives
};

template <typename Tag, typename Type>
struct keyword {
  // NOLINTNEXTLINE(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
  static keyword<Tag, Type> const instance;

  struct tagged_argument<Tag, Type> const
  operator=(Type const& arg) const { // this operator can be used as shortcut to create a tagged_argument
    return tagged_argument<Tag, Type>{arg};
  }
};

template <typename Tag, typename Type>
struct keyword<Tag, Type> const keyword<Tag, Type>::instance = {};

// creates a new variable
#define AR_DEFINE_TAGGED_TYPE(newType, tagType, tagValueType) using newType = ::arangodb::tagged_argument<tagType, tagValueType>;
#define AR_DEFINE_KEYWORD(newKeyWord, tagType, tagValueType) ::arangodb::keyword<tagType, tagValueType> newKeyWord = decltype(newKeyWord)::instance ;

}
