// Copyright 2021 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <vector>
#include "throw_exception.hpp"

namespace bh = boost::histogram;
using uogrowth_t = decltype(bh::axis::option::growth | bh::axis::option::underflow |
                            bh::axis::option::overflow);

using arg_t = boost::variant2::variant<std::vector<int>, int>;

int main() {
  using axis_type =
      bh::axis::regular<double, bh::use_default, bh::use_default, uogrowth_t>;
  using axis_variant_type = bh::axis::variant<axis_type>;

  // 1D growing A
  {
    auto axes = std::vector<axis_variant_type>({axis_type(10, 0, 1)});
    auto h = bh::make_histogram_with(bh::dense_storage<double>(), axes);
    auto h2 = h;

    h(-1);

    // used to crash, while growing B did not crash
    std::vector<arg_t> vargs = {-1};
    h2.fill(vargs);

    BOOST_TEST_EQ(h, h2);
  }

  // 1D growing B
  {
    auto axes = std::vector<axis_variant_type>({axis_type(10, 0, 1)});
    auto h = bh::make_histogram_with(bh::dense_storage<double>(), axes);
    auto h2 = h;

    h(2);
    h(-1);

    std::vector<int> f1({2});
    std::vector<int> f2({-1});

    h2.fill(f1);
    h2.fill(f2);

    BOOST_TEST_EQ(h, h2);
  }

  return boost::report_errors();
}
