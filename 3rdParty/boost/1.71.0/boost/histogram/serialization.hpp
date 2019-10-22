// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_SERIALIZATION_HPP
#define BOOST_HISTOGRAM_SERIALIZATION_HPP

#include <boost/archive/archive_exception.hpp>
#include <boost/assert.hpp>
#include <boost/histogram/accumulators/mean.hpp>
#include <boost/histogram/accumulators/sum.hpp>
#include <boost/histogram/accumulators/weighted_mean.hpp>
#include <boost/histogram/accumulators/weighted_sum.hpp>
#include <boost/histogram/axis/category.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/axis/variable.hpp>
#include <boost/histogram/axis/variant.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <boost/histogram/unsafe_access.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/tuple.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/throw_exception.hpp>
#include <boost/serialization/vector.hpp>
#include <tuple>
#include <type_traits>

/**
  \file boost/histogram/serialization.hpp

  Implemenations of the serialization functions using
  [Boost.Serialization](https://www.boost.org/doc/libs/develop/libs/serialization/doc/index.html).
 */

#ifndef BOOST_HISTOGRAM_DOXYGEN_INVOKED

namespace std {
template <class Archive, class... Ts>
void serialize(Archive& ar, tuple<Ts...>& t, unsigned /* version */) {
  ::boost::mp11::tuple_for_each(
      t, [&ar](auto& x) { ar& boost::serialization::make_nvp("item", x); });
}
} // namespace std

namespace boost {
namespace histogram {

namespace accumulators {
template <class RealType>
template <class Archive>
void sum<RealType>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("large", large_);
  ar& serialization::make_nvp("small", small_);
}

template <class RealType>
template <class Archive>
void weighted_sum<RealType>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("sum_of_weights", sum_of_weights_);
  ar& serialization::make_nvp("sum_of_weights_squared", sum_of_weights_squared_);
}

template <class RealType>
template <class Archive>
void mean<RealType>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("sum", sum_);
  ar& serialization::make_nvp("mean", mean_);
  ar& serialization::make_nvp("sum_of_deltas_squared", sum_of_deltas_squared_);
}

template <class RealType>
template <class Archive>
void weighted_mean<RealType>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("sum_of_weights", sum_of_weights_);
  ar& serialization::make_nvp("sum_of_weights_squared", sum_of_weights_squared_);
  ar& serialization::make_nvp("weighted_mean", weighted_mean_);
  ar& serialization::make_nvp("sum_of_weighted_deltas_squared",
                              sum_of_weighted_deltas_squared_);
}

template <class Archive, class T>
void serialize(Archive& ar, thread_safe<T>& t, unsigned /* version */) {
  T value = t;
  ar& serialization::make_nvp("value", value);
  t = value;
}
} // namespace accumulators

namespace axis {

namespace transform {
template <class Archive>
void serialize(Archive&, id&, unsigned /* version */) {}

template <class Archive>
void serialize(Archive&, log&, unsigned /* version */) {}

template <class Archive>
void serialize(Archive&, sqrt&, unsigned /* version */) {}

template <class Archive>
void serialize(Archive& ar, pow& t, unsigned /* version */) {
  ar& serialization::make_nvp("power", t.power);
}
} // namespace transform

template <class Archive>
void serialize(Archive&, null_type&, unsigned /* version */) {}

template <class T, class Tr, class M, class O>
template <class Archive>
void regular<T, Tr, M, O>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("transform", static_cast<transform_type&>(*this));
  ar& serialization::make_nvp("size", size_meta_.first());
  ar& serialization::make_nvp("meta", size_meta_.second());
  ar& serialization::make_nvp("min", min_);
  ar& serialization::make_nvp("delta", delta_);
}

template <class T, class M, class O>
template <class Archive>
void integer<T, M, O>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("size", size_meta_.first());
  ar& serialization::make_nvp("meta", size_meta_.second());
  ar& serialization::make_nvp("min", min_);
}

template <class T, class M, class O, class A>
template <class Archive>
void variable<T, M, O, A>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("seq", vec_meta_.first());
  ar& serialization::make_nvp("meta", vec_meta_.second());
}

template <class T, class M, class O, class A>
template <class Archive>
void category<T, M, O, A>::serialize(Archive& ar, unsigned /* version */) {
  ar& serialization::make_nvp("seq", vec_meta_.first());
  ar& serialization::make_nvp("meta", vec_meta_.second());
}

// variant_proxy is a workaround to remain backward compatible in the serialization
// format. It uses only the public interface of axis::variant for serialization and
// therefore works independently of the underlying variant implementation.
template <class Variant>
struct variant_proxy {
  Variant& v;

  BOOST_SERIALIZATION_SPLIT_MEMBER()

  template <class Archive>
  void save(Archive& ar, unsigned /* version */) const {
    visit(
        [&ar](auto& value) {
          using T = std::decay_t<decltype(value)>;
          int which = static_cast<int>(mp11::mp_find<Variant, T>::value);
          ar << serialization::make_nvp("which", which);
          ar << serialization::make_nvp("value", value);
        },
        v);
  }

  template <class Archive>
  void load(Archive& ar, unsigned /* version */) {
    int which = 0;
    ar >> serialization::make_nvp("which", which);
    constexpr unsigned N = mp11::mp_size<Variant>::value;
    if (which < 0 || static_cast<unsigned>(which) >= N)
      // throw on invalid which, which >= N can happen if type was removed from variant
      serialization::throw_exception(
          archive::archive_exception(archive::archive_exception::unsupported_version));
    mp11::mp_with_index<N>(static_cast<unsigned>(which), [&ar, this](auto i) {
      using T = mp11::mp_at_c<Variant, i>;
      T value;
      ar >> serialization::make_nvp("value", value);
      v = std::move(value);
      T* new_address = get_if<T>(&v);
      ar.reset_object_address(new_address, &value);
    });
  }
};

template <class Archive, class... Ts>
void serialize(Archive& ar, variant<Ts...>& v, unsigned /* version */) {
  variant_proxy<variant<Ts...>> p{v};
  ar& serialization::make_nvp("variant", p);
}
} // namespace axis

namespace detail {
template <class Archive, class T>
void serialize(Archive& ar, vector_impl<T>& impl, unsigned /* version */) {
  ar& serialization::make_nvp("vector", static_cast<T&>(impl));
}

template <class Archive, class T>
void serialize(Archive& ar, array_impl<T>& impl, unsigned /* version */) {
  ar& serialization::make_nvp("size", impl.size_);
  ar& serialization::make_nvp("array",
                              serialization::make_array(&impl.front(), impl.size_));
}

template <class Archive, class T>
void serialize(Archive& ar, map_impl<T>& impl, unsigned /* version */) {
  ar& serialization::make_nvp("size", impl.size_);
  ar& serialization::make_nvp("map", static_cast<T&>(impl));
}

template <class Archive, class Allocator>
void serialize(Archive& ar, large_int<Allocator>& x, unsigned /* version */) {
  ar& serialization::make_nvp("data", x.data);
}
} // namespace detail

template <class Archive, class T>
void serialize(Archive& ar, storage_adaptor<T>& x, unsigned /* version */) {
  auto& impl = unsafe_access::storage_adaptor_impl(x);
  ar& serialization::make_nvp("impl", impl);
}

template <class Allocator, class Archive>
void serialize(Archive& ar, unlimited_storage<Allocator>& s, unsigned /* version */) {
  auto& buffer = unsafe_access::unlimited_storage_buffer(s);
  using buffer_t = std::remove_reference_t<decltype(buffer)>;
  if (Archive::is_loading::value) {
    buffer_t helper(buffer.alloc);
    std::size_t size;
    ar& serialization::make_nvp("type", helper.type);
    ar& serialization::make_nvp("size", size);
    helper.visit([&buffer, size](auto* tp) {
      BOOST_ASSERT(tp == nullptr);
      using T = std::decay_t<decltype(*tp)>;
      buffer.template make<T>(size);
    });
  } else {
    ar& serialization::make_nvp("type", buffer.type);
    ar& serialization::make_nvp("size", buffer.size);
  }
  buffer.visit([&buffer, &ar](auto* tp) {
    using T = std::decay_t<decltype(*tp)>;
    ar& serialization::make_nvp(
        "buffer",
        serialization::make_array(reinterpret_cast<T*>(buffer.ptr), buffer.size));
  });
}

template <class Archive, class A, class S>
void serialize(Archive& ar, histogram<A, S>& h, unsigned /* version */) {
  ar& serialization::make_nvp("axes", unsafe_access::axes(h));
  ar& serialization::make_nvp("storage", unsafe_access::storage(h));
}

} // namespace histogram
} // namespace boost

#endif

#endif
