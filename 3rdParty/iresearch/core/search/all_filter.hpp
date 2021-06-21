////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ALL_FILTER_H
#define IRESEARCH_ALL_FILTER_H

#include "filter.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @class all
/// @brief filter that returns all documents
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API all: public filter {
 public:
  DECLARE_FACTORY();

  all() noexcept;

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& reader,
    const order::prepared& order,
    boost_t filter_boost,
    const attribute_provider* ctx) const override;
}; // all

} // ROOT

#endif
