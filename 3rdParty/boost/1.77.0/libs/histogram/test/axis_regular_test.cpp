// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/regular.hpp>
#include <limits>
#include <sstream>
#include <type_traits>
#include "is_close.hpp"
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_axis.hpp"
#include "utility_str.hpp"

int main() {
  using namespace boost::histogram;
  using def = use_default;
  namespace tr = axis::transform;

  BOOST_TEST(std::is_nothrow_move_assignable<axis::regular<>>::value);
  BOOST_TEST(std::is_nothrow_move_constructible<axis::regular<>>::value);

  // bad_ctors
  {
    BOOST_TEST_THROWS(axis::regular<>(1, 0, 0), std::invalid_argument);
    BOOST_TEST_THROWS(axis::regular<>(0, 0, 1), std::invalid_argument);
  }

  // ctors and assignment
  {
    axis::regular<> a{4, -2, 2};
    axis::regular<> b;
    BOOST_TEST_NE(a, b);
    b = a;
    BOOST_TEST_EQ(a, b);
    axis::regular<> c = std::move(b);
    BOOST_TEST_EQ(c, a);
    axis::regular<> d;
    BOOST_TEST_NE(c, d);
    d = std::move(c);
    BOOST_TEST_EQ(d, a);
  }

  // input, output
  {
    axis::regular<> a{4, -2, 2, "foo"};
    BOOST_TEST_EQ(a.metadata(), "foo");
    const auto& cref = a;
    BOOST_TEST_EQ(cref.metadata(), "foo");
    cref.metadata() = "bar"; // this is allowed
    BOOST_TEST_EQ(cref.metadata(), "bar");
    BOOST_TEST_EQ(a.value(0), -2);
    BOOST_TEST_EQ(a.value(1), -1);
    BOOST_TEST_EQ(a.value(2), 0);
    BOOST_TEST_EQ(a.value(3), 1);
    BOOST_TEST_EQ(a.value(4), 2);
    BOOST_TEST_EQ(a.bin(-1).lower(), -std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.bin(-1).upper(), -2);
    BOOST_TEST_EQ(a.bin(a.size()).lower(), 2);
    BOOST_TEST_EQ(a.bin(a.size()).upper(), std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.index(-10.), -1);
    BOOST_TEST_EQ(a.index(-2.1), -1);
    BOOST_TEST_EQ(a.index(-2.0), 0);
    BOOST_TEST_EQ(a.index(-1.1), 0);
    BOOST_TEST_EQ(a.index(0.0), 2);
    BOOST_TEST_EQ(a.index(0.9), 2);
    BOOST_TEST_EQ(a.index(1.0), 3);
    BOOST_TEST_EQ(a.index(10.), 4);
    BOOST_TEST_EQ(a.index(-std::numeric_limits<double>::infinity()), -1);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::infinity()), 4);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::quiet_NaN()), 4);

    BOOST_TEST_EQ(str(a),
                  "regular(4, -2, 2, metadata=\"bar\", options=underflow | overflow)");
  }

  // with inverted range
  {
    axis::regular<> a{2, 1, -2};
    BOOST_TEST_EQ(a.bin(-1).lower(), std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.bin(0).lower(), 1);
    BOOST_TEST_EQ(a.bin(1).lower(), -0.5);
    BOOST_TEST_EQ(a.bin(2).lower(), -2);
    BOOST_TEST_EQ(a.bin(2).upper(), -std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.index(2), -1);
    BOOST_TEST_EQ(a.index(1.001), -1);
    BOOST_TEST_EQ(a.index(1), 0);
    BOOST_TEST_EQ(a.index(0), 0);
    BOOST_TEST_EQ(a.index(-0.499), 0);
    BOOST_TEST_EQ(a.index(-0.5), 1);
    BOOST_TEST_EQ(a.index(-1), 1);
    BOOST_TEST_EQ(a.index(-2), 2);
    BOOST_TEST_EQ(a.index(-20), 2);
  }

  // with log transform
  {
    auto a = axis::regular<double, tr::log>{2, 1e0, 1e2};
    BOOST_TEST_EQ(a.bin(-1).lower(), 0.0);
    BOOST_TEST_IS_CLOSE(a.bin(0).lower(), 1.0, 1e-9);
    BOOST_TEST_IS_CLOSE(a.bin(1).lower(), 10.0, 1e-9);
    BOOST_TEST_IS_CLOSE(a.bin(2).lower(), 100.0, 1e-9);
    BOOST_TEST_EQ(a.bin(2).upper(), std::numeric_limits<double>::infinity());

    BOOST_TEST_EQ(a.index(-1), 2); // produces NaN in conversion
    BOOST_TEST_EQ(a.index(0), -1);
    BOOST_TEST_EQ(a.index(1), 0);
    BOOST_TEST_EQ(a.index(9), 0);
    BOOST_TEST_EQ(a.index(10), 1);
    BOOST_TEST_EQ(a.index(90), 1);
    BOOST_TEST_EQ(a.index(100), 2);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::infinity()), 2);

    BOOST_TEST_THROWS((axis::regular<double, tr::log>{2, -1, 0}), std::invalid_argument);

    BOOST_TEST_CSTR_EQ(
        str(a).c_str(),
        "regular(transform::log{}, 2, 1, 100, options=underflow | overflow)");
  }

  // with sqrt transform
  {
    axis::regular<double, tr::sqrt> a(2, 0, 4);
    // this is weird, but -inf * -inf = inf, thus the lower bound
    BOOST_TEST_EQ(a.bin(-1).lower(), std::numeric_limits<double>::infinity());
    BOOST_TEST_IS_CLOSE(a.bin(0).lower(), 0.0, 1e-9);
    BOOST_TEST_IS_CLOSE(a.bin(1).lower(), 1.0, 1e-9);
    BOOST_TEST_IS_CLOSE(a.bin(2).lower(), 4.0, 1e-9);
    BOOST_TEST_EQ(a.bin(2).upper(), std::numeric_limits<double>::infinity());

    BOOST_TEST_EQ(a.index(-1), 2); // produces NaN in conversion
    BOOST_TEST_EQ(a.index(0), 0);
    BOOST_TEST_EQ(a.index(0.99), 0);
    BOOST_TEST_EQ(a.index(1), 1);
    BOOST_TEST_EQ(a.index(3.99), 1);
    BOOST_TEST_EQ(a.index(4), 2);
    BOOST_TEST_EQ(a.index(100), 2);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::infinity()), 2);

    BOOST_TEST_EQ(str(a),
                  "regular(transform::sqrt{}, 2, 0, 4, options=underflow | overflow)");
  }

  // with pow transform
  {
    axis::regular<double, tr::pow> a(tr::pow{0.5}, 2, 0, 4);
    // this is weird, but -inf * -inf = inf, thus the lower bound
    BOOST_TEST_EQ(a.bin(-1).lower(), std::numeric_limits<double>::infinity());
    BOOST_TEST_IS_CLOSE(a.bin(0).lower(), 0.0, 1e-9);
    BOOST_TEST_IS_CLOSE(a.bin(1).lower(), 1.0, 1e-9);
    BOOST_TEST_IS_CLOSE(a.bin(2).lower(), 4.0, 1e-9);
    BOOST_TEST_EQ(a.bin(2).upper(), std::numeric_limits<double>::infinity());

    BOOST_TEST_EQ(a.index(-1), 2); // produces NaN in conversion
    BOOST_TEST_EQ(a.index(0), 0);
    BOOST_TEST_EQ(a.index(0.99), 0);
    BOOST_TEST_EQ(a.index(1), 1);
    BOOST_TEST_EQ(a.index(3.99), 1);
    BOOST_TEST_EQ(a.index(4), 2);
    BOOST_TEST_EQ(a.index(100), 2);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::infinity()), 2);

    BOOST_TEST_EQ(str(a),
                  "regular(transform::pow{0.5}, 2, 0, 4, options=underflow | overflow)");
  }

  // with step
  {
    axis::regular<> a(axis::step(0.5), 1, 3);
    BOOST_TEST_EQ(a.size(), 4);
    BOOST_TEST_EQ(a.bin(-1).lower(), -std::numeric_limits<double>::infinity());
    BOOST_TEST_EQ(a.value(0), 1);
    BOOST_TEST_EQ(a.value(1), 1.5);
    BOOST_TEST_EQ(a.value(2), 2);
    BOOST_TEST_EQ(a.value(3), 2.5);
    BOOST_TEST_EQ(a.value(4), 3);
    BOOST_TEST_EQ(a.bin(4).upper(), std::numeric_limits<double>::infinity());

    axis::regular<> b(axis::step(0.5), 1, 3.1);
    BOOST_TEST_EQ(a, b);
  }

  // with circular option
  {
    axis::circular<> a{4, 0, 1};
    BOOST_TEST_EQ(a.bin(-1).lower(), a.bin(a.size() - 1).lower() - 1);
    BOOST_TEST_EQ(a.index(-1.0 * 3), 0);
    BOOST_TEST_EQ(a.index(0.0), 0);
    BOOST_TEST_EQ(a.index(0.25), 1);
    BOOST_TEST_EQ(a.index(0.5), 2);
    BOOST_TEST_EQ(a.index(0.75), 3);
    BOOST_TEST_EQ(a.index(1.0), 0);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::infinity()), 4);
    BOOST_TEST_EQ(a.index(-std::numeric_limits<double>::infinity()), 4);
    BOOST_TEST_EQ(a.index(std::numeric_limits<double>::quiet_NaN()), 4);
  }

  // with growth
  {
    using pii_t = std::pair<axis::index_type, axis::index_type>;
    axis::regular<double, def, def, axis::option::growth_t> a{1, 0, 1};
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(a.update(0), pii_t(0, 0));
    BOOST_TEST_EQ(a.size(), 1);
    BOOST_TEST_EQ(a.update(1), pii_t(1, -1));
    BOOST_TEST_EQ(a.size(), 2);
    BOOST_TEST_EQ(a.value(0), 0);
    BOOST_TEST_EQ(a.value(2), 2);
    BOOST_TEST_EQ(a.update(-1), pii_t(0, 1));
    BOOST_TEST_EQ(a.size(), 3);
    BOOST_TEST_EQ(a.value(0), -1);
    BOOST_TEST_EQ(a.value(3), 2);
    BOOST_TEST_EQ(a.update(-10), pii_t(0, 9));
    BOOST_TEST_EQ(a.size(), 12);
    BOOST_TEST_EQ(a.value(0), -10);
    BOOST_TEST_EQ(a.value(12), 2);
    BOOST_TEST_EQ(a.update(std::numeric_limits<double>::infinity()), pii_t(a.size(), 0));
    BOOST_TEST_EQ(a.update(std::numeric_limits<double>::quiet_NaN()), pii_t(a.size(), 0));
    BOOST_TEST_EQ(a.update(-std::numeric_limits<double>::infinity()), pii_t(-1, 0));
  }

  // iterators
  {
    test_axis_iterator(axis::regular<>(5, 0, 1), 0, 5);
    test_axis_iterator(axis::regular<double, def, def, axis::option::none_t>(5, 0, 1), 0,
                       5);
    test_axis_iterator(axis::circular<>(5, 0, 1), 0, 5);
  }

  // bin_type streamable
  {
    auto test = [](const auto& x, const char* ref) {
      std::ostringstream os;
      os << x;
      BOOST_TEST_EQ(os.str(), std::string(ref));
    };

    auto a = axis::regular<>(2, 0, 1);
    test(a.bin(0), "[0, 0.5)");
  }

  // null_type streamable
  {
    auto a = axis::regular<float, def, axis::null_type>(2, 0, 1);
    BOOST_TEST_EQ(str(a), "regular(2, 0, 1, options=underflow | overflow)");
  }

  // shrink and rebin
  {
    using A = axis::regular<>;
    auto a = A(5, 0, 5);
    auto b = A(a, 1, 4, 1);
    BOOST_TEST_EQ(b.size(), 3);
    BOOST_TEST_EQ(b.value(0), 1);
    BOOST_TEST_EQ(b.value(3), 4);
    auto c = A(a, 0, 4, 2);
    BOOST_TEST_EQ(c.size(), 2);
    BOOST_TEST_EQ(c.value(0), 0);
    BOOST_TEST_EQ(c.value(2), 4);
    auto e = A(a, 1, 5, 2);
    BOOST_TEST_EQ(e.size(), 2);
    BOOST_TEST_EQ(e.value(0), 1);
    BOOST_TEST_EQ(e.value(2), 5);
  }

  // shrink and rebin with circular option
  {
    using A = axis::circular<>;
    auto a = A(4, 1, 5);
    BOOST_TEST_THROWS(A(a, 1, 4, 1), std::invalid_argument);
    BOOST_TEST_THROWS(A(a, 0, 3, 1), std::invalid_argument);
    auto b = A(a, 0, 4, 2);
    BOOST_TEST_EQ(b.size(), 2);
    BOOST_TEST_EQ(b.value(0), 1);
    BOOST_TEST_EQ(b.value(2), 5);
  }

  return boost::report_errors();
}
