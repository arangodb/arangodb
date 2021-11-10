////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <memory>
#include <ostream>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of
// data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift
// intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/box.hpp>
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include "Containers/ImmerMemoryPolicy.h"
#include "Logger/LogMacros.h"

namespace arangodb {

struct LoggableValue {
  virtual ~LoggableValue() = default;
  virtual auto operator<<(std::ostream& os) const noexcept -> std::ostream& = 0;
};

template<typename T, const char* N>
struct LogNameValuePair : LoggableValue {
  explicit LogNameValuePair(T t) : value(std::move(t)) {}
  T value;
  auto operator<<(std::ostream& os) const noexcept -> std::ostream& override {
    return os << N << "=" << value;
  }
};

struct LoggerContext {
  explicit LoggerContext(LogTopic const& topic) : topic(topic) {}

  template<const char N[], typename T>
  auto with(T&& t) const -> LoggerContext {
    using S = std::decay_t<T>;
    auto pair = std::make_shared<LogNameValuePair<S, N>>(std::forward<T>(t));
    return LoggerContext(values.push_back(std::move(pair)), topic);
  }

  auto withTopic(LogTopic const& newTopic) const {
    return LoggerContext(values, newTopic);
  }

  friend auto operator<<(std::ostream& os, LoggerContext const& ctx)
      -> std::ostream& {
    os << "[";
    bool first = true;
    for (auto const& v : ctx.values) {
      if (!first) {
        os << ", ";
      }
      v->operator<<(os);
      first = false;
    }
    os << "]";
    return os;
  }

  using Container = ::immer::flex_vector<std::shared_ptr<LoggableValue>,
                                         arangodb::immer::arango_memory_policy>;
  LogTopic const& topic;
  Container const values = {};

 private:
  LoggerContext(Container values, LogTopic const& topic)
      : topic(topic), values(std::move(values)) {}
};
}  // namespace arangodb

#define LOG_CTX(id, level, ctx) \
  LOG_TOPIC(id, level, (ctx).topic) << (ctx) << " "
#define LOG_CTX_IF(id, level, ctx, cond) \
  LOG_TOPIC_IF(id, level, (ctx).topic, cond) << (ctx) << " "
