////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

namespace arangodb {
namespace agency {

namespace detail {
template <typename T>
struct moving_ptr {
  moving_ptr(T* p) : _ptr(p) {}
  ~moving_ptr() = default;

  moving_ptr& operator=(moving_ptr const&) = delete;
  moving_ptr(moving_ptr const&) = delete;

  moving_ptr& operator=(moving_ptr&& o) {
    reset(o.release());
    return *this;
  };
  moving_ptr(moving_ptr&& o) { *this = std::move(o); }

  T* release() {
    T* result = _ptr;
    _ptr = nullptr;
    return result;
  }
  void reset(T* p) { _ptr = p; }

  T* operator->() { return _ptr; }
  T* get() { return _ptr; }
  T& operator*() { return *_ptr; }
  operator bool() { return _ptr != nullptr; }

 private:
  T* _ptr = nullptr;
};

template <typename V>
void add_to_builder(VPackBuilder* b, V const& v) {
  b->add(VPackValue(v));
}

template <>
inline void add_to_builder(VPackBuilder* b, VPackSlice const& v) {
  b->add(v);
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
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      return *this;
    }
    ~read_trx() try {
      if (_builder) {
        end();
      }
    } catch (...) {
    }
    read_trx(read_trx&&) = default;
    read_trx& operator=(read_trx&&) = default;

   private:
    friend envelope;
    read_trx(builder_ptr b) : _builder(std::move(b)) { _builder->openArray(); }
    builder_ptr _builder;
  };

  struct write_trx;
  struct precs_trx {
    template <typename K, typename V>
    precs_trx&& isEqual(K&& k, V&& v) && {
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      _builder->openObject();
      detail::add_to_builder(_builder.get(), "old");
      detail::add_to_builder(_builder.get(), std::forward<V>(v));
      _builder->close();
      return std::move(*this);
    }

    template <typename K>
    precs_trx&& isEmpty(K&& k) && {
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      _builder->openObject();
      _builder->add("oldEmpty", VPackValue(true));
      _builder->close();
      return std::move(*this);
    }

    envelope end() {
      _builder->close();
      _builder->add(VPackValue(AgencyWriteTransaction::randomClientId()));
      return envelope(_builder.release());
    }
    ~precs_trx() try {
      if (_builder) {
        end();
      }
    } catch (...) {
    }
    precs_trx(precs_trx&&) = default;
    precs_trx& operator=(precs_trx&&) = default;

   private:
    friend write_trx;
    precs_trx(builder_ptr b) : _builder(std::move(b)) { _builder->openObject(); }
    builder_ptr _builder;
  };

  struct write_trx {
    envelope end() {
      _builder->close();
      _builder->add(VPackSlice::emptyObjectSlice());
      _builder->add(VPackValue(AgencyWriteTransaction::randomClientId()));
      return envelope(_builder.release());
    }
    template <typename K, typename V>
    write_trx&& key(K&& k, V&& v) {
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      detail::add_to_builder(_builder.get(), std::forward<V>(v));
      return std::move(*this);
    }
    template <typename K, typename F>
    write_trx&& emplace(K&& k, F f) {
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      _builder->openObject();
      f(*_builder);
      _builder->close();
      return std::move(*this);
    }

    template <typename K, typename V>
    write_trx&& set(K&& k, V&& v) {
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      _builder->openObject();
      _builder->add("op", VPackValue("set"));
      this->set("new", std::forward<V>(v));
      _builder->close();
      return std::move(*this);
    }

    template <typename K>
    write_trx&& remove(K&& k) {
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      _builder->openObject();
      _builder->add("op", VPackValue("delete"));
      _builder->close();
      return std::move(*this);
    }

    template <typename K>
    write_trx&& inc(K&& k, uint64_t delta = 1) && {
      detail::add_to_builder(_builder.get(), std::forward<K>(k));
      _builder->openObject();
      this->key("op", "increment");
      this->key("delta", delta);
      _builder->close();
      return std::move(*this);
    }

    ~write_trx() try {
      if (_builder) {
        end();
      }
    } catch (...) {
    }
    write_trx(write_trx&&) = default;
    write_trx& operator=(write_trx&&) = default;


    precs_trx precs() { return precs_trx(std::move(_builder)); }

   private:
    friend envelope;
    write_trx(builder_ptr b) : _builder(std::move(b)) {
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
  ~envelope() try {
    if (_builder) {
      done();
    }
  } catch (...) {
  }
  envelope(envelope&&) = default;
  envelope& operator=(envelope&&) = default;

  static envelope into_builder(VPackBuilder& b) {
    b.openArray();
    return envelope(&b);
  }

 private:
  envelope(VPackBuilder* b) : _builder(b) {}
  builder_ptr _builder;
};
#if 0
template <typename B>
struct envelope;

template <typename T>
struct buffer_mapper;

template <typename B, typename T, typename... Vs>
struct read_trx {
  using buffer = buffer_mapper<B>;

  template <typename K>
  read_trx key(K&& k) && {
    _buffer.setValue(std::forward<K>(k));
    return read_trx(std::move(_buffer));
  }

  T done() && {
    _buffer.closeArray();
    return T(std::move(_buffer));
  }

 private:
  friend T;
  explicit read_trx(buffer&& b) : _buffer(std::move(b)) {}
  buffer _buffer;
};

template <typename B, typename T>
struct write_trx;

template <typename B, typename T>
struct precs_trx {
  using buffer = buffer_mapper<B>;

  template <typename K, typename V>
  precs_trx isEqual(K&& k, V&& v) && {
    _buffer.setValue(std::forward<K>(k));
    _buffer.openObject();
    _buffer.setKey("old", std::forward<V>(v));
    _buffer.closeObject();
    return precs_trx(std::move(_buffer));
  }

  template <typename K>
  precs_trx isEmpty(K&& k) && {
    _buffer.setValue(std::forward<K>(k));
    _buffer.openObject();
    _buffer.setKey("oldEmpty", true);
    _buffer.closeObject();
    return precs_trx(std::move(_buffer));
  }

  T done() && {
    _buffer.closeObject();
    _buffer.setValue(VPackValue(AgencyWriteTransaction::randomClientId()));
    _buffer.closeArray();
    return T(std::move(_buffer));
  }

 private:
  friend write_trx<B, T>;
  friend T;

  precs_trx(buffer&& b) : _buffer(std::move(b)) {}
  buffer _buffer;
};

template <typename B, typename T>
struct write_trx {
  using buffer = buffer_mapper<B>;

  template <typename K, typename V>
  write_trx set(K&& k, V&& v) && {
    _buffer.setValue(std::forward<K>(k));
    _buffer.openObject();
    _buffer.setKey("op", "set");
    _buffer.setKey("new", std::forward<V>(v));
    _buffer.closeObject();
    return write_trx(std::move(_buffer));
  }

  template <typename K, typename F>
  write_trx emplace(K&& k, F f) && {
    _buffer.setValue(std::forward<K>(k));
    _buffer.openObject();
    f(_buffer.userObject());
    _buffer.closeObject();
    return write_trx(std::move(_buffer));
  }

  template <typename K>
  write_trx remove(K&& k) && {
    _buffer.setValue(std::forward<K>(k));
    _buffer.openObject();
    _buffer.setKey("op", "delete");
    _buffer.closeObject();
    return write_trx(std::move(_buffer));
  }

  template <typename K>
  write_trx inc(K&& k, uint64_t delta = 1) && {
    _buffer.setValue(std::forward<K>(k));
    _buffer.openObject();
    _buffer.setKey("op", "increment");
    _buffer.setKey("delta", delta);
    _buffer.closeObject();
    return write_trx(std::move(_buffer));
  }

  T done() && {
    _buffer.closeObject();
    _buffer.setValue(VPackSlice::emptyObjectSlice());
    _buffer.setValue(VPackValue(AgencyWriteTransaction::randomClientId()));
    _buffer.closeArray();
    return T(std::move(_buffer));
  }

  precs_trx<B, T> precs() && {
    _buffer.closeObject();
    _buffer.openObject();
    return std::move(_buffer);
  }
  write_trx& operator=(write_trx&&) = default;

 private:
  friend T;

  explicit write_trx(buffer&& b) : _buffer(std::move(b)) {}
  buffer _buffer;
};

template <typename B>
struct envelope {
  using buffer = buffer_mapper<B>;

  read_trx<B, envelope> read() && {
    _buffer.openArray();
    return read_trx<B, envelope>(std::move(_buffer));
  }

  write_trx<B, envelope> write() && {
    _buffer.openArray();
    _buffer.openObject();
    return write_trx<B, envelope>(std::move(_buffer));
  }

  void done() && { _buffer.closeArray(); }

  static envelope create(VPackBuilder& b) {
    buffer buff(b);
    envelope env(buffer{b});
    env._buffer.openArray();
    return env;
  }

 private:
  friend write_trx<B, envelope>;
  friend read_trx<B, envelope>;
  friend precs_trx<B, envelope>;
  explicit envelope(buffer&& b) : _buffer(std::move(b)) {}
  envelope() = default;
  buffer _buffer;
};

namespace detail {

template <typename V>
void add_to_builder(VPackBuilder* b, V const& v) {
  b->add(VPackValue(v));
}

template <>
inline void add_to_builder(VPackBuilder* b, VPackSlice const& v) {
  b->add(v);
}

}  // namespace detail

template <>
struct buffer_mapper<VPackBuilder> {
  explicit buffer_mapper(VPackBuilder& builder) : _builder(&builder){};

  template <typename K, typename V>
  void setKey(K&& k, V&& v) {
    setValue(std::forward<K>(k));
    setValue(std::forward<V>(v));
  }

  template <typename K>
  void setValue(K const& k) {
    detail::add_to_builder(_builder, k);
  }

  void openArray() { _builder->openArray(); }
  void closeArray() { _builder->close(); }
  void openObject() { _builder->openObject(); }
  void closeObject() { _builder->close(); }

  VPackBuilder& userObject() { return *_builder; }

  VPackBuilder* _builder;
};
#endif
}  // namespace agency
}  // namespace arangodb
#endif
