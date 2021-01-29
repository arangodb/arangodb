////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_BOOST_H
#define IRESEARCH_BOOST_H

#include "scorers.hpp"

namespace iresearch {

struct IRESEARCH_API boost_sort final : public sort {
  static constexpr string_ref type_name() noexcept {
    return "boostscore";
  }

  DECLARE_FACTORY(); // for use with irs::order::add<T>() and default args

  static void init();

  boost_sort() noexcept;

  virtual sort::prepared::ptr prepare() const override;
}; // boost_sort

}

#endif // IRESEARCH_BOOST_H

