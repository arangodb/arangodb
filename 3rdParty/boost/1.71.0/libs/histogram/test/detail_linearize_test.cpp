// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/variant.hpp>
#include <boost/histogram/detail/linearize.hpp>
#include <tuple>
#include <utility>
#include <vector>
#include "std_ostream.hpp"

using namespace boost::histogram;

int main() {
  {
    struct growing {
      auto update(int) { return std::make_pair(0, 0); }
    };
    using T = growing;
    using I = axis::integer<>;

    using A = std::tuple<I, T>;
    using B = std::vector<T>;
    using C = std::vector<axis::variant<I, T>>;
    using D = std::tuple<I>;
    using E = std::vector<I>;
    using F = std::vector<axis::variant<I>>;

    BOOST_TEST_TRAIT_TRUE((detail::has_growing_axis<A>));
    BOOST_TEST_TRAIT_TRUE((detail::has_growing_axis<B>));
    BOOST_TEST_TRAIT_TRUE((detail::has_growing_axis<C>));
    BOOST_TEST_TRAIT_FALSE((detail::has_growing_axis<D>));
    BOOST_TEST_TRAIT_FALSE((detail::has_growing_axis<E>));
    BOOST_TEST_TRAIT_FALSE((detail::has_growing_axis<F>));
  }

  {
    struct multidim {
      auto index(const std::tuple<int>&) { return 0; }
    };
    using T = multidim;
    using I = axis::integer<>;

    using A = std::tuple<I, T>;
    using B = std::vector<T>;
    using C = std::vector<axis::variant<I, T>>;
    using D = std::tuple<I>;
    using E = std::vector<I>;
    using F = std::vector<axis::variant<I>>;

    BOOST_TEST_TRAIT_TRUE((detail::has_multidim_axis<A>));
    BOOST_TEST_TRAIT_TRUE((detail::has_multidim_axis<B>));
    BOOST_TEST_TRAIT_TRUE((detail::has_multidim_axis<C>));
    BOOST_TEST_TRAIT_FALSE((detail::has_multidim_axis<D>));
    BOOST_TEST_TRAIT_FALSE((detail::has_multidim_axis<E>));
    BOOST_TEST_TRAIT_FALSE((detail::has_multidim_axis<F>));
  }
  return boost::report_errors();
}
