////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "misc.hpp"
#include "noncopyable.hpp"
#include "std.hpp"
#include "utils/assert.hpp"

namespace irs {

template<typename T, typename Base = memory::Managed>
struct iterator : Base {
  virtual T value() const = 0;
  // TODO(MBkkt) return T? In such case some algorithms probably will be faster
  virtual bool next() = 0;
};

template<typename Key, typename Value, typename Iterator, typename Base,
         typename Less = std::less<Key>>
class iterator_adaptor : public Base {
 public:
  typedef Iterator iterator_type;
  typedef Key key_type;
  typedef Value value_type;
  typedef const value_type& const_reference;

  iterator_adaptor(iterator_type begin, iterator_type end,
                   const Less& less = Less())
    : begin_{begin}, cur_{begin}, end_{end}, less_{less} {}

  const_reference value() const noexcept final { return *cur_; }

  bool seek(key_type key) noexcept final {
    begin_ = std::lower_bound(cur_, end_, key, less_);
    return next();
  }

  bool next() noexcept final {
    if (begin_ == end_) {
      cur_ = begin_;  // seal iterator
      return false;
    }

    cur_ = begin_++;
    return true;
  }

 private:
  iterator_type begin_;
  iterator_type cur_;
  iterator_type end_;
  IRS_NO_UNIQUE_ADDRESS Less less_;
};

}  // namespace irs
