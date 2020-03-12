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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ATTRIBUTE_RANGE_H
#define IRESEARCH_ATTRIBUTE_RANGE_H

#include "attributes.hpp"

NS_ROOT

template<typename Adapter>
struct attribute_range_state {
  virtual Adapter* get_next_iterator() = 0;
  virtual void reset_next_iterator_state() = 0;
  virtual ~attribute_range_state() = default;
};

template<typename Adapter>
class attribute_range final : public attribute {
 public:
  DECLARE_TYPE_ID(attribute::type_id);

  void set_state(attribute_range_state<Adapter>* ars) noexcept {
    ars_ = ars;
  }

  Adapter* value() noexcept {
    return value_;
  }

  bool next();

  void reset() {
    value_ = nullptr;
    assert(ars_);
    ars_->reset_next_iterator_state();
  }

 private:
  attribute_range_state<Adapter>* ars_ = nullptr;
  Adapter* value_ = nullptr;
}; // attribute_range

NS_END // ROOT

#endif
