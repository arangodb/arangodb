// Copyright 2018-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/detail/large_int.hpp>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include "std_ostream.hpp"

namespace boost {
namespace histogram {
namespace detail {
template <class Allocator>
std::ostream& operator<<(std::ostream& os, const large_int<Allocator>& x) {
  os << "large_int" << x.data;
  return os;
}
} // namespace detail
} // namespace histogram
} // namespace boost

using namespace boost::histogram;

using large_int = detail::large_int<std::allocator<std::uint64_t>>;

template <class... Ts>
auto make_large_int(Ts... ts) {
  large_int r;
  r.data = {static_cast<uint64_t>(ts)...};
  return r;
}

int main() {
  // low-level tools
  {
    uint8_t c = 0;
    BOOST_TEST_EQ(detail::safe_increment(c), true);
    BOOST_TEST_EQ(c, 1);
    c = 255;
    BOOST_TEST_EQ(detail::safe_increment(c), false);
    BOOST_TEST_EQ(c, 255);
    c = 0;
    BOOST_TEST_EQ(detail::safe_radd(c, 255u), true);
    BOOST_TEST_EQ(c, 255);
    c = 1;
    BOOST_TEST_EQ(detail::safe_radd(c, 255u), false);
    BOOST_TEST_EQ(c, 1);
    c = 255;
    BOOST_TEST_EQ(detail::safe_radd(c, 1u), false);
    BOOST_TEST_EQ(c, 255);
  }

  const auto vmax = std::numeric_limits<std::uint64_t>::max();

  // ctors, assign
  {
    large_int a(42);
    large_int b(a);
    BOOST_TEST_EQ(b.data.front(), 42);
    large_int c(std::move(b));
    BOOST_TEST_EQ(c.data.front(), 42);
    large_int d, e;
    d = a;
    BOOST_TEST_EQ(d.data.front(), 42);
    e = std::move(c);
    BOOST_TEST_EQ(e.data.front(), 42);
  }

  // comparison
  {
    BOOST_TEST_EQ(large_int(), 0u);
    BOOST_TEST_EQ(large_int(1u), 1u);
    BOOST_TEST_EQ(large_int(1u), 1.0);
    BOOST_TEST_EQ(large_int(1u), large_int(1u));
    BOOST_TEST_NE(large_int(1u), 2u);
    BOOST_TEST_NE(large_int(1u), 2.0);
    BOOST_TEST_NE(large_int(1u), 2.0f);
    BOOST_TEST_NE(large_int(1u), large_int(2u));
    BOOST_TEST_LT(large_int(1u), 2u);
    BOOST_TEST_LT(large_int(1u), 2.0);
    BOOST_TEST_LT(large_int(1u), 2.0f);
    BOOST_TEST_LT(large_int(1u), large_int(2u));
    BOOST_TEST_LE(large_int(1u), 2u);
    BOOST_TEST_LE(large_int(1u), 2.0);
    BOOST_TEST_LE(large_int(1u), 2.0f);
    BOOST_TEST_LE(large_int(1u), large_int(2u));
    BOOST_TEST_LE(large_int(1u), 1u);
    BOOST_TEST_GT(large_int(1u), 0u);
    BOOST_TEST_GT(large_int(1u), 0.0);
    BOOST_TEST_GT(large_int(1u), 0.0f);
    BOOST_TEST_GT(large_int(1u), large_int(0u));
    BOOST_TEST_GE(large_int(1u), 0u);
    BOOST_TEST_GE(large_int(1u), 0.0);
    BOOST_TEST_GE(large_int(1u), 0.0f);
    BOOST_TEST_GE(large_int(1u), 1u);
    BOOST_TEST_GE(large_int(1u), large_int(0u));
    BOOST_TEST_NOT(large_int(1u) < large_int(1u));
    BOOST_TEST_NOT(large_int(1u) > large_int(1u));

    BOOST_TEST_GT(1, large_int());
    BOOST_TEST_LT(-1, large_int());
    BOOST_TEST_GE(1, large_int());
    BOOST_TEST_LE(-1, large_int());
    BOOST_TEST_NE(1, large_int());

    constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
    BOOST_TEST_NOT(large_int(1u) < nan);
    BOOST_TEST_NOT(large_int(1u) > nan);
    BOOST_TEST_NOT(large_int(1u) == nan);
    BOOST_TEST(large_int(1u) != nan); // same behavior as int compared to nan
    BOOST_TEST_NOT(large_int(1u) <= nan);
    BOOST_TEST_NOT(large_int(1u) >= nan);

    BOOST_TEST_NOT(nan < large_int(1u));
    BOOST_TEST_NOT(nan > large_int(1u));
    BOOST_TEST_NOT(nan == large_int(1u));
    BOOST_TEST(nan != large_int(1u)); // same behavior as int compared to nan
    BOOST_TEST_NOT(nan <= large_int(1u));
    BOOST_TEST_NOT(nan >= large_int(1u));
  }

  // increment
  {
    auto a = large_int();
    ++a;
    BOOST_TEST_EQ(a.data.size(), 1);
    BOOST_TEST_EQ(a.data[0], 1);
    ++a;
    BOOST_TEST_EQ(a.data[0], 2);
    a = vmax;
    BOOST_TEST_EQ(a, vmax);
    BOOST_TEST_EQ(a, static_cast<double>(vmax));
    ++a;
    BOOST_TEST_EQ(a, make_large_int(0, 1));
    ++a;
    BOOST_TEST_EQ(a, make_large_int(1, 1));
    a += a;
    BOOST_TEST_EQ(a, make_large_int(2, 2));
    BOOST_TEST_EQ(a, 2 * static_cast<double>(vmax) + 2);

    // carry once A
    a.data[0] = vmax;
    a.data[1] = 1;
    ++a;
    BOOST_TEST_EQ(a, make_large_int(0, 2));
    // carry once B
    a.data[0] = vmax;
    a.data[1] = 1;
    a += 1;
    BOOST_TEST_EQ(a, make_large_int(0, 2));
    // carry once C
    a.data[0] = vmax;
    a.data[1] = 1;
    a += make_large_int(1, 1);
    BOOST_TEST_EQ(a, make_large_int(0, 3));

    a.data[0] = vmax - 1;
    a.data[1] = vmax;
    ++a;
    BOOST_TEST_EQ(a, make_large_int(vmax, vmax));

    // carry two times A
    ++a;
    BOOST_TEST_EQ(a, make_large_int(0, 0, 1));
    // carry two times B
    a = make_large_int(vmax, vmax);
    a += 1;
    BOOST_TEST_EQ(a, make_large_int(0, 0, 1));
    // carry two times C
    a = make_large_int(vmax, vmax);
    a += large_int(1);
    BOOST_TEST_EQ(a, make_large_int(0, 0, 1));

    // carry and enlarge
    a = make_large_int(vmax, vmax);
    a += a;
    BOOST_TEST_EQ(a, make_large_int(vmax - 1, vmax, 1));

    // add smaller to larger
    a = make_large_int(1, 1, 1);
    a += make_large_int(1, 1);
    BOOST_TEST_EQ(a, make_large_int(2, 2, 1));

    // add larger to smaller
    a = make_large_int(1, 1);
    a += make_large_int(1, 1, 1);
    BOOST_TEST_EQ(a, make_large_int(2, 2, 1));

    a = large_int(1);
    auto b = 1.0;
    BOOST_TEST_EQ(a, b);
    for (unsigned i = 0; i < 80; ++i) {
      b += b;
      BOOST_TEST_NE(a, b);
      a += a;
      BOOST_TEST_EQ(a, b);
    }
    BOOST_TEST_GT(a.data.size(), 1u);
  }

  return boost::report_errors();
}
