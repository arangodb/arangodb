// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <array>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/axis/variable.hpp>
#include <boost/histogram/axis/variant.hpp>
#include <boost/histogram/detail/detect.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <deque>
#include <initializer_list>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "throw_exception.hpp"
#include "utility_allocator.hpp"
#include "std_ostream.hpp"

using namespace boost::histogram;
using namespace boost::histogram::detail;

int main() {
  // has_method_value*
  {
    struct A {};
    struct B {
      A value(int) const { return {}; }
    };
    struct C {
      char value(int) const { return 0; }
    };

    BOOST_TEST_TRAIT_FALSE((has_method_value<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_value<B>));
    BOOST_TEST_TRAIT_TRUE((has_method_value<C>));
  }

  // has_method_options
  {
    struct A {};
    struct B {
      void options() {}
    };

    BOOST_TEST_TRAIT_FALSE((has_method_options<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_options<B>));
  }

  // has_method_metadata
  {
    struct A {};
    struct B {
      void metadata();
    };

    BOOST_TEST_TRAIT_FALSE((has_method_metadata<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_metadata<B>));
  }

  // has_method_update
  {
    struct A {};
    struct B {
      void update(int) {}
    };
    using C = axis::integer<int, axis::null_type, use_default>;

    BOOST_TEST_TRAIT_FALSE((has_method_update<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_update<B>));
    BOOST_TEST_TRAIT_TRUE((has_method_update<C>));
  }

  // has_method_resize
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::map<int, int>;

    BOOST_TEST_TRAIT_FALSE((has_method_resize<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_resize<B>));
    BOOST_TEST_TRAIT_FALSE((has_method_resize<C>));
  }

  // has_method_size
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::map<int, int>;

    BOOST_TEST_TRAIT_FALSE((has_method_size<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_size<B>));
    BOOST_TEST_TRAIT_TRUE((has_method_size<C>));
  }

  // has_method_clear
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::map<int, int>;
    using D = std::array<int, 10>;

    BOOST_TEST_TRAIT_FALSE((has_method_clear<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_clear<B>));
    BOOST_TEST_TRAIT_TRUE((has_method_clear<C>));
    BOOST_TEST_TRAIT_FALSE((has_method_clear<D>));
  }

  // has_allocator
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::map<int, int>;
    using D = std::array<int, 10>;

    BOOST_TEST_TRAIT_FALSE((has_method_clear<A>));
    BOOST_TEST_TRAIT_TRUE((has_method_clear<B>));
    BOOST_TEST_TRAIT_TRUE((has_method_clear<C>));
    BOOST_TEST_TRAIT_FALSE((has_method_clear<D>));
  }

  // is_storage
  {
    struct A {};
    using B = std::vector<int>;
    using C = unlimited_storage<>;

    BOOST_TEST_TRAIT_FALSE((is_storage<A>));
    BOOST_TEST_TRAIT_FALSE((is_storage<B>));
    BOOST_TEST_TRAIT_TRUE((is_storage<C>));
  }

  // is_indexable
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::map<int, int>;
    using D = std::map<A, int>;

    BOOST_TEST_TRAIT_FALSE((is_indexable<A>));
    BOOST_TEST_TRAIT_TRUE((is_indexable<B>));
    BOOST_TEST_TRAIT_TRUE((is_indexable<C>));
    BOOST_TEST_TRAIT_FALSE((is_indexable<D>));
  }

  // is_transform
  {
    struct A {};
    struct B {
      double forward(double);
      double inverse(double);
    };

    BOOST_TEST_TRAIT_FALSE((is_transform<A>));
    BOOST_TEST_TRAIT_TRUE((is_transform<B>));
  }

  // is_vector_like
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::array<int, 10>;
    using D = std::map<unsigned, int>;
    using E = std::deque<int>;
    BOOST_TEST_TRAIT_FALSE((is_vector_like<A>));
    BOOST_TEST_TRAIT_TRUE((is_vector_like<B>));
    BOOST_TEST_TRAIT_FALSE((is_vector_like<C>));
    BOOST_TEST_TRAIT_FALSE((is_vector_like<D>));
    BOOST_TEST_TRAIT_TRUE((is_vector_like<E>));
  }

  // is_array_like
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::array<int, 10>;
    using D = std::map<unsigned, int>;
    BOOST_TEST_TRAIT_FALSE((is_array_like<A>));
    BOOST_TEST_TRAIT_FALSE((is_array_like<B>));
    BOOST_TEST_TRAIT_TRUE((is_array_like<C>));
    BOOST_TEST_TRAIT_FALSE((is_array_like<D>));
  }

  // is_map_like
  {
    struct A {};
    using B = std::vector<int>;
    using C = std::array<int, 10>;
    using D = std::map<unsigned, int>;
    using E = std::unordered_map<unsigned, int>;
    BOOST_TEST_TRAIT_FALSE((is_map_like<A>));
    BOOST_TEST_TRAIT_FALSE((is_map_like<B>));
    BOOST_TEST_TRAIT_FALSE((is_map_like<C>));
    BOOST_TEST_TRAIT_TRUE((is_map_like<D>));
    BOOST_TEST_TRAIT_TRUE((is_map_like<E>));
  }

  // is_axis
  {
    struct A {};
    struct B {
      int index(double);
      int size() const;
    };
    struct C {
      int index(double);
    };
    struct D {
      int size();
    };
    using E = axis::variant<axis::regular<>>;

    BOOST_TEST_TRAIT_FALSE((is_axis<A>));
    BOOST_TEST_TRAIT_TRUE((is_axis<B>));
    BOOST_TEST_TRAIT_FALSE((is_axis<C>));
    BOOST_TEST_TRAIT_FALSE((is_axis<D>));
    BOOST_TEST_TRAIT_FALSE((is_axis<E>));
  }

  // is_iterable
  {
    using A = std::vector<int>;
    using B = int[3];
    using C = std::initializer_list<int>;
    BOOST_TEST_TRAIT_FALSE((is_iterable<int>));
    BOOST_TEST_TRAIT_TRUE((is_iterable<A>));
    BOOST_TEST_TRAIT_TRUE((is_iterable<B>));
    BOOST_TEST_TRAIT_TRUE((is_iterable<C>));
  }

  // is_streamable
  {
    struct Foo {};
    BOOST_TEST_TRAIT_TRUE((is_streamable<int>));
    BOOST_TEST_TRAIT_TRUE((is_streamable<std::string>));
    BOOST_TEST_TRAIT_FALSE((is_streamable<Foo>));
  }

  // is_axis_variant
  {
    struct A {};
    BOOST_TEST_TRAIT_FALSE((is_axis_variant<A>));
    BOOST_TEST_TRAIT_TRUE((is_axis_variant<axis::variant<>>));
    BOOST_TEST_TRAIT_TRUE((is_axis_variant<axis::variant<axis::regular<>>>));
  }

  // is_sequence_of_axis
  {
    using A = std::vector<axis::regular<>>;
    using B = std::vector<axis::variant<axis::regular<>>>;
    using C = std::vector<int>;
    auto v = std::vector<axis::variant<axis::regular<>, axis::integer<>>>();
    BOOST_TEST_TRAIT_TRUE((is_sequence_of_any_axis<A>));
    BOOST_TEST_TRAIT_TRUE((is_sequence_of_axis<A>));
    BOOST_TEST_TRAIT_FALSE((is_sequence_of_axis_variant<A>));
    BOOST_TEST_TRAIT_TRUE((is_sequence_of_any_axis<B>));
    BOOST_TEST_TRAIT_TRUE((is_sequence_of_axis_variant<B>));
    BOOST_TEST_TRAIT_FALSE((is_sequence_of_axis<B>));
    BOOST_TEST_TRAIT_FALSE((is_sequence_of_any_axis<C>));
    BOOST_TEST_TRAIT_TRUE((is_sequence_of_any_axis<decltype(v)>));
  }

  // is_weight
  {
    struct A {};
    using B = int;
    using C = weight_type<int>;
    BOOST_TEST_TRAIT_FALSE((is_weight<A>));
    BOOST_TEST_TRAIT_FALSE((is_weight<B>));
    BOOST_TEST_TRAIT_TRUE((is_weight<C>));
  }

  // is_sample
  {
    struct A {};
    using B = int;
    using C = sample_type<int>;
    BOOST_TEST_TRAIT_FALSE((is_sample<A>));
    BOOST_TEST_TRAIT_FALSE((is_sample<B>));
    BOOST_TEST_TRAIT_TRUE((is_sample<C>));
  }

  // has_operator_equal
  {
    struct A {};
    struct B {
      bool operator==(const B&) const { return true; }
    };

    BOOST_TEST_TRAIT_FALSE((has_operator_equal<A, A>));
    BOOST_TEST_TRAIT_FALSE((has_operator_equal<B, A>));
    BOOST_TEST_TRAIT_TRUE((has_operator_equal<B, B>));
    BOOST_TEST_TRAIT_TRUE((has_operator_equal<const B&, const B&>));
  }

  // has_operator_radd
  {
    struct A {};
    struct B {
      B& operator+=(const B&) { return *this; }
    };

    BOOST_TEST_TRAIT_FALSE((has_operator_radd<A, A>));
    BOOST_TEST_TRAIT_FALSE((has_operator_radd<B, A>));
    BOOST_TEST_TRAIT_TRUE((has_operator_radd<B, B>));
    BOOST_TEST_TRAIT_TRUE((has_operator_radd<B&, const B&>));
  }

  return boost::report_errors();
}
