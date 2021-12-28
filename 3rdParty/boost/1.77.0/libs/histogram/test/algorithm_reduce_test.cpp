// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/algorithm/reduce.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <boost/histogram/axis/category.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <boost/histogram/axis/variable.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/unsafe_access.hpp>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;
using namespace boost::histogram::algorithm;

struct unreducible {
  axis::index_type index(int) const { return 0; }
  axis::index_type size() const { return 1; }
  friend std::ostream& operator<<(std::ostream& os, const unreducible&) {
    os << "unreducible";
    return os;
  }
};

template <typename Tag>
void run_tests() {
  // limitations: shrink does not work with arguments not convertible to double

  using R = axis::regular<double>;
  using ID = axis::integer<double, axis::empty_type>;
  using V = axis::variable<double, axis::empty_type>;
  using CI = axis::category<int, axis::empty_type>;

  // various failures
  {
    auto h = make(Tag(), R(4, 1, 5), R(3, -1, 2));

    // not allowed: invalid axis index
    BOOST_TEST_THROWS((void)reduce(h, slice(10, 2, 3)), std::invalid_argument);
    // two slice requests for same axis not allowed
    BOOST_TEST_THROWS((void)reduce(h, slice(1, 0, 2), slice(1, 1, 3)),
                      std::invalid_argument);
    // two rebin requests for same axis not allowed
    BOOST_TEST_THROWS((void)reduce(h, rebin(0, 2), rebin(0, 2)), std::invalid_argument);
    // rebin and slice_and_rebin with merge > 1 requests for same axis cannot be fused
    BOOST_TEST_THROWS((void)reduce(h, slice_and_rebin(0, 1, 3, 2), rebin(0, 2)),
                      std::invalid_argument);
    BOOST_TEST_THROWS((void)reduce(h, shrink(1, 0, 2), crop(1, 0, 2)),
                      std::invalid_argument);
    // not allowed: slice with begin >= end
    BOOST_TEST_THROWS((void)reduce(h, slice(0, 1, 1)), std::invalid_argument);
    BOOST_TEST_THROWS((void)reduce(h, slice(0, 2, 1)), std::invalid_argument);
    // not allowed: shrink with lower == upper
    BOOST_TEST_THROWS((void)reduce(h, shrink(0, 0, 0)), std::invalid_argument);
    // not allowed: crop with lower == upper
    BOOST_TEST_THROWS((void)reduce(h, crop(0, 0, 0)), std::invalid_argument);
    // not allowed: shrink axis to zero size
    BOOST_TEST_THROWS((void)reduce(h, shrink(0, 10, 11)), std::invalid_argument);
    // not allowed: rebin with zero merge
    BOOST_TEST_THROWS((void)reduce(h, rebin(0, 0)), std::invalid_argument);
    // not allowed: reducing unreducible axis
    BOOST_TEST_THROWS((void)reduce(make(Tag(), unreducible{}), slice(0, 1)),
                      std::invalid_argument);
  }

  // shrink and crop behavior when value on edge and not on edge is inclusive:
  // - lower edge of shrink: pick bin which contains edge, lower <= x < upper
  // - upper edge of shrink: pick bin which contains edge + 1, lower < x <= upper
  {
    auto h = make(Tag(), ID(0, 3));
    const auto& ax = h.axis();
    BOOST_TEST_EQ(ax.value(0), 0);
    BOOST_TEST_EQ(ax.value(3), 3);
    BOOST_TEST_EQ(ax.index(-1), -1);
    BOOST_TEST_EQ(ax.index(3), 3);

    BOOST_TEST_EQ(reduce(h, shrink(-1, 5)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, shrink(0, 3)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, shrink(1, 3)).axis(), ID(1, 3));
    BOOST_TEST_EQ(reduce(h, shrink(1.001, 3)).axis(), ID(1, 3));
    BOOST_TEST_EQ(reduce(h, shrink(1.999, 3)).axis(), ID(1, 3));
    BOOST_TEST_EQ(reduce(h, shrink(2, 3)).axis(), ID(2, 3));
    BOOST_TEST_EQ(reduce(h, shrink(0, 2.999)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, shrink(0, 2.001)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, shrink(0, 2)).axis(), ID(0, 2));
    BOOST_TEST_EQ(reduce(h, shrink(0, 1.999)).axis(), ID(0, 2));

    BOOST_TEST_EQ(reduce(h, crop(-1, 5)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, crop(0, 3)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, crop(1, 3)).axis(), ID(1, 3));
    BOOST_TEST_EQ(reduce(h, crop(1.001, 3)).axis(), ID(1, 3));
    BOOST_TEST_EQ(reduce(h, crop(1.999, 3)).axis(), ID(1, 3));
    BOOST_TEST_EQ(reduce(h, crop(2, 3)).axis(), ID(2, 3));
    BOOST_TEST_EQ(reduce(h, crop(0, 2.999)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, crop(0, 2.001)).axis(), ID(0, 3));
    BOOST_TEST_EQ(reduce(h, crop(0, 2)).axis(), ID(0, 2));
    BOOST_TEST_EQ(reduce(h, crop(0, 1.999)).axis(), ID(0, 2));
  }

  // shrink and rebin
  {
    auto h = make_s(Tag(), std::vector<int>(), R(4, 1, 5, "1"), R(3, -1, 2, "2"));

    /*
      matrix layout:
      x ->
    y 1 0 1 0
    | 1 1 0 0
    v 0 2 1 3
    */
    h.at(0, 0) = 1;
    h.at(0, 1) = 1;
    h.at(1, 1) = 1;
    h.at(1, 2) = 2;
    h.at(2, 0) = 1;
    h.at(2, 2) = 1;
    h.at(3, 2) = 3;
    h.at(-1, -1) = 1; // underflow
    h.at(4, 3) = 1;   // overflow

    // should do nothing, index order does not matter
    auto hr = reduce(h, shrink(1, -1, 2), rebin(0, 1));
    BOOST_TEST_EQ(hr.rank(), 2);
    BOOST_TEST_EQ(sum(hr), 12);
    BOOST_TEST_EQ(hr.axis(0), R(4, 1, 5, "1"));
    BOOST_TEST_EQ(hr.axis(1), R(3, -1, 2, "2"));
    BOOST_TEST_EQ(hr, h);

    // noop slice
    hr = reduce(h, slice(1, 0, 4), slice(0, 0, 4));
    BOOST_TEST_EQ(hr, h);

    // shrinking along first axis
    hr = reduce(h, shrink(0, 2, 4));
    BOOST_TEST_EQ(hr.rank(), 2);
    BOOST_TEST_EQ(sum(hr), 12);
    BOOST_TEST_EQ(hr.axis(0), R(2, 2, 4, "1"));
    BOOST_TEST_EQ(hr.axis(1), R(3, -1, 2, "2"));
    BOOST_TEST_EQ(hr.at(-1, 0), 1); // underflow
    BOOST_TEST_EQ(hr.at(0, 0), 0);
    BOOST_TEST_EQ(hr.at(1, 0), 1);
    BOOST_TEST_EQ(hr.at(2, 0), 0); // overflow
    BOOST_TEST_EQ(hr.at(-1, 1), 1);
    BOOST_TEST_EQ(hr.at(0, 1), 1);
    BOOST_TEST_EQ(hr.at(1, 1), 0);
    BOOST_TEST_EQ(hr.at(2, 1), 0);
    BOOST_TEST_EQ(hr.at(-1, 2), 0);
    BOOST_TEST_EQ(hr.at(0, 2), 2);
    BOOST_TEST_EQ(hr.at(1, 2), 1);
    BOOST_TEST_EQ(hr.at(2, 2), 3);

    /*
      matrix layout:
      x
    y 1 0 1 0
      1 1 0 0
      0 2 1 3
    */

    hr = reduce(h, shrink_and_rebin(0, 2, 5, 2), rebin(1, 3));
    BOOST_TEST_EQ(hr.rank(), 2);
    BOOST_TEST_EQ(sum(hr), 12);
    BOOST_TEST_EQ(hr.axis(0), R(1, 2, 4, "1"));
    BOOST_TEST_EQ(hr.axis(1), R(1, -1, 2, "2"));
    BOOST_TEST_EQ(hr.at(-1, 0), 2); // underflow
    BOOST_TEST_EQ(hr.at(0, 0), 5);
    BOOST_TEST_EQ(hr.at(1, 0), 3); // overflow

    // test overload that accepts iterable and test option fusion
    std::vector<reduce_command> opts{{shrink(0, 2, 5), rebin(0, 2), rebin(1, 3)}};
    auto hr2 = reduce(h, opts);
    BOOST_TEST_EQ(hr2, hr);
    reduce_command opts2[3] = {rebin(1, 3), rebin(0, 2), shrink(0, 2, 5)};
    auto hr3 = reduce(h, opts2);
    BOOST_TEST_EQ(hr3, hr);

    // test positional args
    auto hr4 = reduce(h, shrink_and_rebin(2, 5, 2), rebin(3));
    BOOST_TEST_EQ(hr4, hr);
  }

  // crop and rebin
  {
    auto h = make_s(Tag(), std::vector<int>(), R(4, 1, 5), R(3, 1, 4));

    /*
      matrix layout:
      x ->
    y 1 0 1 0
    | 1 1 0 0
    v 0 2 1 3
    */
    h.at(0, 0) = 1;
    h.at(0, 1) = 1;
    h.at(1, 1) = 1;
    h.at(1, 2) = 2;
    h.at(2, 0) = 1;
    h.at(2, 2) = 1;
    h.at(3, 2) = 3;
    h.at(-1, -1) = 1; // underflow
    h.at(4, 3) = 1;   // overflow

    /*
      crop first and last column in x and y
      matrix layout after:
      x
    y 3 1
    */

    auto hr = reduce(h, crop(2, 4), crop_and_rebin(2, 4, 2));
    BOOST_TEST_EQ(hr.rank(), 2);
    BOOST_TEST_EQ(sum(hr), 4);
    BOOST_TEST_EQ(hr.axis(0), R(2, 2, 4));
    BOOST_TEST_EQ(hr.axis(1), R(1, 2, 4));
    BOOST_TEST_EQ(hr.at(0, 0), 3);
    BOOST_TEST_EQ(hr.at(1, 0), 1);

    // slice with crop mode
    auto hr2 = reduce(h, slice(1, 3, slice_mode::crop),
                      slice_and_rebin(1, 3, 2, slice_mode::crop));
    BOOST_TEST_EQ(hr, hr2);

    // explicit axis indices
    auto hr3 = reduce(h, crop_and_rebin(1, 2, 4, 2), crop(0, 2, 4));
    BOOST_TEST_EQ(hr, hr3);
    auto hr4 = reduce(h, slice_and_rebin(1, 1, 3, 2, slice_mode::crop),
                      slice(0, 1, 3, slice_mode::crop));
    BOOST_TEST_EQ(hr, hr4);
  }

  // one-sided crop
  {
    auto h = make_s(Tag(), std::vector<int>(), ID(1, 4));
    std::fill(h.begin(), h.end(), 1);
    // underflow: 1
    // index 0, x 1: 1
    // index 1, x 2: 1
    // index 2, x 3: 1
    // overflow: 1
    BOOST_TEST_EQ(sum(h), 5);
    BOOST_TEST_EQ(h.size(), 5);

    // keep underflow
    auto hr1 = reduce(h, crop(0, 3));
    BOOST_TEST_EQ(hr1, reduce(h, slice(-1, 2, slice_mode::crop)));
    BOOST_TEST_EQ(sum(hr1), 3);
    BOOST_TEST_EQ(hr1.size(), 4); // flow bins are not physically removed, only zeroed

    // remove underflow
    auto hr2 = reduce(h, crop(1, 3));
    BOOST_TEST_EQ(hr2, reduce(h, slice(0, 2, slice_mode::crop)));
    BOOST_TEST_EQ(sum(hr2), 2);
    BOOST_TEST_EQ(hr2.size(), 4); // flow bins are not physically removed, only zeroed

    // keep overflow
    auto hr3 = reduce(h, crop(2, 5));
    BOOST_TEST_EQ(hr3, reduce(h, slice(1, 4, slice_mode::crop)));
    BOOST_TEST_EQ(sum(hr3), 3);
    BOOST_TEST_EQ(hr3.size(), 4); // flow bins are not physically removed, only zeroed

    // remove overflow
    auto hr4 = reduce(h, crop(2, 4));
    BOOST_TEST_EQ(hr4, reduce(h, slice(1, 3, slice_mode::crop)));
    BOOST_TEST_EQ(sum(hr4), 2);
    BOOST_TEST_EQ(hr4.size(), 4); // flow bins are not physically removed, only zeroed
  }

  // one-sided crop and rebin with regular axis
  {
    auto h = make_s(Tag(), std::vector<int>(), R(4, 1, 5));
    std::fill(h.begin(), h.end(), 1);
    // underflow: 1
    // index 0, x [1, 2): 1
    // index 1, x [2, 3): 1
    // index 2, x [3, 4): 1
    // index 3, x [4, 5): 1
    // overflow: 1
    BOOST_TEST_EQ(sum(h), 6);
    BOOST_TEST_EQ(h.size(), 6);

    // keep underflow
    auto hr1 = reduce(h, crop_and_rebin(0, 5, 2));
    BOOST_TEST_EQ(sum(hr1), 5);
    BOOST_TEST_EQ(hr1.size(), 4); // flow bins are not physically removed, only zeroed
    auto hr2 = reduce(h, crop_and_rebin(0, 3, 2));
    BOOST_TEST_EQ(sum(hr2), 3);
    BOOST_TEST_EQ(hr2.size(), 3); // flow bins are not physically removed, only zeroed

    // remove underflow but keep overflow
    auto hr3 = reduce(h, crop_and_rebin(1, 6, 2));
    BOOST_TEST_EQ(sum(hr3), 5);
    BOOST_TEST_EQ(hr3.size(), 4); // flow bins are not physically removed, only zeroed

    // remove underflow and overflow
    auto hr4 = reduce(h, crop_and_rebin(1, 3, 2));
    BOOST_TEST_EQ(sum(hr4), 2);
    BOOST_TEST_EQ(hr4.size(), 3); // flow bins are not physically removed, only zeroed
  }

  // mixed axis types
  {
    R r(5, 0.0, 5.0);
    V v{{1., 2., 3.}};
    CI c{{1, 2, 3}};
    unreducible u;

    auto h = make(Tag(), r, v, c, u);
    auto hr = algorithm::reduce(h, shrink(0, 2, 4), slice(2, 1, 3));
    BOOST_TEST_EQ(hr.axis(0), (R{2, 2, 4}));
    BOOST_TEST_EQ(hr.axis(1), (V{{1., 2., 3.}}));
    BOOST_TEST_EQ(hr.axis(2), (CI{{2, 3}}));
    BOOST_TEST_EQ(hr.axis(3), u);
    BOOST_TEST_THROWS((void)algorithm::reduce(h, rebin(2, 2)), std::invalid_argument);
  }

  // reduce on integer axis, rebin must fail
  {
    auto h = make(Tag(), axis::integer<>(1, 4));
    BOOST_TEST_THROWS((void)reduce(h, rebin(2)), std::invalid_argument);
    auto hr = reduce(h, shrink(2, 3));
    BOOST_TEST_EQ(hr.axis().size(), 1);
    BOOST_TEST_EQ(hr.axis().bin(0), 2);
    BOOST_TEST_EQ(hr.axis().bin(1), 3);
  }

  // reduce on circular axis, shrink must fail, also rebin with remainder
  {
    auto h = make(Tag(), axis::circular<>(4, 1, 4));
    BOOST_TEST_THROWS((void)reduce(h, shrink(0, 2)), std::invalid_argument);
    BOOST_TEST_THROWS((void)reduce(h, rebin(3)), std::invalid_argument);
    auto hr = reduce(h, rebin(2));
    BOOST_TEST_EQ(hr.axis().size(), 2);
    BOOST_TEST_EQ(hr.axis().bin(0).lower(), 1);
    BOOST_TEST_EQ(hr.axis().bin(1).upper(), 4);
  }

  // reduce on variable axis
  {
    auto h = make(Tag(), V({0, 1, 2, 3, 4, 5, 6}));
    auto hr = reduce(h, shrink_and_rebin(1, 5, 2));
    BOOST_TEST_EQ(hr.axis().size(), 2);
    BOOST_TEST_EQ(hr.axis().value(0), 1);
    BOOST_TEST_EQ(hr.axis().value(1), 3);
    BOOST_TEST_EQ(hr.axis().value(2), 5);
  }

  // reduce on axis with inverted range
  {
    auto h = make(Tag(), R(4, 2, -2));
    const auto& ax = h.axis();
    BOOST_TEST_EQ(ax.index(-0.999), 2);
    BOOST_TEST_EQ(ax.index(-1.0), 3);
    BOOST_TEST_EQ(ax.index(-1.5), 3);

    BOOST_TEST_EQ(reduce(h, shrink(3, -3)).axis(), R(4, 2, -2));
    BOOST_TEST_EQ(reduce(h, shrink(2, -2)).axis(), R(4, 2, -2));
    BOOST_TEST_EQ(reduce(h, shrink(1.999, -2)).axis(), R(4, 2, -2));
    BOOST_TEST_EQ(reduce(h, shrink(1.001, -2)).axis(), R(4, 2, -2));
    BOOST_TEST_EQ(reduce(h, shrink(1, -2)).axis(), R(3, 1, -2));
    BOOST_TEST_EQ(reduce(h, shrink(2, -1.999)).axis(), R(4, 2, -2));
    BOOST_TEST_EQ(reduce(h, shrink(2, -1.001)).axis(), R(4, 2, -2));
    BOOST_TEST_EQ(reduce(h, shrink(2, -1)).axis(), R(3, 2, -1));
  }

  // reduce on histogram with axis without flow bins, see GitHub issue #257
  {
    auto h = make(Tag(), axis::integer<int, use_default, axis::option::underflow_t>(0, 3),
                  axis::integer<int, use_default, axis::option::overflow_t>(0, 3));

    std::fill(h.begin(), h.end(), 1);

    /*
    Original histogram:
                x
         -1  0  1  2
       -------------
      0|  1  1  1  1
    x 1|  1  1  1  1
      2|  1  1  1  1
      3|  1  1  1  1

    Shrunk histogram:
         -1  0
       -------
      0|  2  1
      1|  4  2
    */

    auto hr = reduce(h, slice(0, 1, 2), slice(1, 1, 2));
    BOOST_TEST_EQ(hr.size(), 2 * 2);
    BOOST_TEST_EQ(hr.axis(0).size(), 1);
    BOOST_TEST_EQ(hr.axis(1).size(), 1);
    BOOST_TEST_EQ(hr.axis(0).bin(0), 1);
    BOOST_TEST_EQ(hr.axis(1).bin(0), 1);

    BOOST_TEST_EQ(hr.at(-1, 0), 2);
    BOOST_TEST_EQ(hr.at(0, 0), 1);
    BOOST_TEST_EQ(hr.at(-1, 1), 4);
    BOOST_TEST_EQ(hr.at(0, 1), 2);
  }

  // reduce on category axis: removed bins are added to overflow bin
  {
    auto h = make(Tag(), CI{{1, 2, 3}});
    std::fill(h.begin(), h.end(), 1);
    // original: [1: 1, 2: 1, 3: 1, overflow: 1]
    auto hr = reduce(h, slice(1, 2));
    // reduced: [2: 1, overflow: 3]
    BOOST_TEST_EQ(hr[0], 1);
    BOOST_TEST_EQ(hr[1], 3);
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
