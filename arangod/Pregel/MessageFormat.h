////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_MFORMAT_H
#define ARANGODB_PREGEL_MFORMAT_H 1

#include <cstddef>
#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace pregel {

template <typename M>
struct MessageFormat {
  virtual ~MessageFormat() {}
  virtual void unwrapValue(VPackSlice body, M& value) const = 0;
  virtual void addValue(VPackBuilder& arrayBuilder, M const& val) const = 0;
};

struct IntegerMessageFormat : public MessageFormat<int64_t> {
  IntegerMessageFormat() {}
  void unwrapValue(VPackSlice s, int64_t& value) const override {
    value = s.getInt();
  }
  void addValue(VPackBuilder& arrayBuilder, int64_t const& val) const override {
    arrayBuilder.add(VPackValue(val));
  }
};

/*
struct DoubleMessageFormat : public MessageFormat<double> {
  DoubleMessageFormat() {}
  void unwrapValue(VPackSlice s, double& value) const override {
    value = s.getDouble();
  }
  void addValue(VPackBuilder& arrayBuilder, double const& val) const override {
    arrayBuilder.add(VPackValue(val));
  }
};

struct FloatMessageFormat : public MessageFormat<float> {
  FloatMessageFormat() {}
  void unwrapValue(VPackSlice s, float& value) const override {
    value = (float)s.getDouble();
  }
  void addValue(VPackBuilder& arrayBuilder, float const& val) const override {
    arrayBuilder.add(VPackValue(val));
  }
};*/

template <typename M>
struct NumberMessageFormat : public MessageFormat<M> {
  static_assert(std::is_arithmetic<M>::value, "Message type must be numeric");
  NumberMessageFormat() {}
  void unwrapValue(VPackSlice s, M& value) const override {
    value = s.getNumber<M>();
  }
  void addValue(VPackBuilder& arrayBuilder, M const& val) const override {
    arrayBuilder.add(VPackValue(val));
  }
};
}  // namespace pregel
}  // namespace arangodb
#endif
