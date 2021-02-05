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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_AGENCY_TRANSACTION_BUILDER_H
#define ARANGOD_CLUSTER_AGENCY_TRANSACTION_BUILDER_H 1

#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/debugging.h"

#include "AgencyComm.h"

namespace arangodb::agency {

namespace detail {
struct no_op_deleter {
  void operator()(void*) const {};
};

template <typename T>
using moving_ptr = std::unique_ptr<T, no_op_deleter>;

template <typename V>
void add_to_builder(VPackBuilder& b, V const& v) {
  b.add(VPackValue(v));
}

template <>
inline void add_to_builder(VPackBuilder& b, VPackSlice const& v) {
  b.add(v);
}

template <typename K, typename V>
void add_to_builder(VPackBuilder& b, K const& key, V const& v) {
  b.add(key, VPackValue(v));
}

template <typename K>
inline void add_to_builder(VPackBuilder& b, K const& key, VPackSlice const& v) {
  b.add(key, v);
}
}  // namespace detail

struct envelope {
  using builder_ptr = detail::moving_ptr<VPackBuilder>;

  struct read_trx {
    envelope end() {
      _builder->close();
      return envelope(_builder.release());
    }
    template <typename K>
    read_trx& key(K&& k) {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      return *this;
    }
    read_trx(read_trx&&) = default;
    read_trx& operator=(read_trx&&) = default;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // if this assertion triggers you forgot to call `end()`
    ~read_trx() { TRI_ASSERT(_builder == nullptr); }
#endif

   private:
    friend envelope;
    read_trx(builder_ptr b) : _builder(std::move(b)) { _builder->openArray(); }
    builder_ptr _builder;
  };

  struct write_trx;
  struct precs_trx {
    template <typename K, typename V>
    precs_trx&& isEqual(K&& k, V&& v) && {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      _builder->openObject();
      detail::add_to_builder(*_builder.get(), "old");
      detail::add_to_builder(*_builder.get(), std::forward<V>(v));
      _builder->close();
      return std::move(*this);
    }

    template <typename K>
    precs_trx&& isEmpty(K&& k) && {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      _builder->openObject();
      _builder->add("oldEmpty", VPackValue(true));
      _builder->close();
      return std::move(*this);
    }

    envelope end(std::string const& clientId = {}) {
      _builder->close();
      _builder->add(VPackValue(clientId.empty() ? AgencyWriteTransaction::randomClientId() : clientId));
      _builder->close();
      return envelope(_builder.release());
    }
    precs_trx(precs_trx&&) = default;
    precs_trx& operator=(precs_trx&&) = default;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // if this assertion triggers you forgot to call `end()`
    ~precs_trx() { TRI_ASSERT(_builder == nullptr); }
#endif

   private:
    friend write_trx;
    precs_trx(builder_ptr b) : _builder(std::move(b)) {
      _builder->close();
      _builder->openObject();
    }
    builder_ptr _builder;
  };

  struct write_trx {
    envelope end(std::string const& clientId = {}) {
      _builder->close();
      _builder->add(VPackSlice::emptyObjectSlice());
      _builder->add(VPackValue(clientId.empty() ? AgencyWriteTransaction::randomClientId() : clientId));
      _builder->close();
      return envelope(_builder.release());
    }
    template <typename K, typename V>
    write_trx&& key(K&& k, V&& v) {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      detail::add_to_builder(*_builder.get(), std::forward<V>(v));
      return std::move(*this);
    }
    template <typename K, typename F>
    write_trx&& emplace(K&& k, F&& f) {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      _builder->openObject();
      std::forward<F>(f)(*_builder);
      _builder->close();
      return std::move(*this);
    }

    template <typename K, typename V>
    write_trx&& set(K&& k, V&& v) {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      _builder->openObject();
      _builder->add("op", VPackValue("set"));
      detail::add_to_builder(*_builder.get(), "new", std::forward<V>(v));
      _builder->close();
      return std::move(*this);
    }

    template <typename K>
    write_trx&& remove(K&& k) {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      _builder->openObject();
      _builder->add("op", VPackValue("delete"));
      _builder->close();
      return std::move(*this);
    }

    template <typename K>
    write_trx&& inc(K&& k, uint64_t delta = 1) && {
      detail::add_to_builder(*_builder.get(), std::forward<K>(k));
      _builder->openObject();
      this->key("op", "increment");
      this->key("delta", delta);
      _builder->close();
      return std::move(*this);
    }

    write_trx(write_trx&&) = default;
    write_trx& operator=(write_trx&&) = default;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // if this assertion triggers you forgot to call `end()`
    ~write_trx() { TRI_ASSERT(_builder == nullptr); }
#endif

    precs_trx precs() { return precs_trx(std::move(_builder)); }

   private:
    friend envelope;
    write_trx(builder_ptr b) : _builder(std::move(b)) {
      _builder->openArray();
      _builder->openObject();
    }
    builder_ptr _builder;
  };

  read_trx read() { return read_trx(std::move(_builder)); }
  write_trx write() { return write_trx(std::move(_builder)); }

  void done() {
    _builder->close();
    _builder.release();
  }

  envelope(envelope&&) = default;
  envelope& operator=(envelope&&) = default;

  static envelope into_builder(VPackBuilder& b) {
    b.openArray();
    return envelope(&b);
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // if this assertion triggers you forgot to call `done()`
    ~envelope() { TRI_ASSERT(_builder == nullptr); }
#endif
 private:
  envelope(VPackBuilder* b) : _builder(b) {}
  builder_ptr _builder;
};

}  // namespace arangodb
#endif
