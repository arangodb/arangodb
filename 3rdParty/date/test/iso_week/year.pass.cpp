// The MIT License (MIT)
//
// Copyright (c) 2015, 2016 Howard Hinnant
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// class year
// {
// public:
//     explicit constexpr year(int y) noexcept;
//
//     year& operator++()    noexcept;
//     year  operator++(int) noexcept;
//     year& operator--()    noexcept;
//     year  operator--(int) noexcept;
//
//     year& operator+=(const years& y) noexcept;
//     year& operator-=(const years& y) noexcept;
//
//     constexpr explicit operator int() const noexcept;
//     constexpr bool ok() const noexcept;
//
//     static constexpr year min() noexcept;
//     static constexpr year max() noexcept;
// };

// constexpr bool operator==(const year& x, const year& y) noexcept;
// constexpr bool operator!=(const year& x, const year& y) noexcept;
// constexpr bool operator< (const year& x, const year& y) noexcept;
// constexpr bool operator> (const year& x, const year& y) noexcept;
// constexpr bool operator<=(const year& x, const year& y) noexcept;
// constexpr bool operator>=(const year& x, const year& y) noexcept;

// constexpr year  operator+(const year&  x, const years& y) noexcept;
// constexpr year  operator+(const years& x, const year&  y) noexcept;
// constexpr year  operator-(const year&  x, const years& y) noexcept;
// constexpr years operator-(const year&  x, const year&  y) noexcept;

// std::ostream& operator<<(std::ostream& os, const year& y);
// inline namespace literals {
// constexpr year operator "" _y(unsigned long long y) noexcept;
// }

#include "iso_week.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<iso_week::year>{}, "");
static_assert(!std::is_default_constructible<iso_week::year>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::year>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::year>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::year>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::year>{}, "");

static_assert(std::is_trivially_copyable<iso_week::year>{}, "");
static_assert(std::is_standard_layout<iso_week::year>{}, "");
static_assert(std::is_literal_type<iso_week::year>{}, "");

static_assert( std::is_nothrow_constructible<iso_week::year, int>{}, "");
static_assert(!std::is_convertible<int, iso_week::year>{}, "");
static_assert( std::is_nothrow_constructible<int, iso_week::year>{}, "");
static_assert(!std::is_convertible<iso_week::year, int>{}, "");
static_assert(static_cast<int>(iso_week::year{-1}) == -1, "");

int
main()
{
    using namespace iso_week;

    static_assert(year{2015} == 2015_y, "");
    static_assert(int{year{2015}} == 2015, "");
    static_assert(year{2015} != 2016_y, "");
    static_assert(year{2015} < 2016_y, "");
    static_assert(year{2016} > 2015_y, "");
    static_assert(year{2015} <= 2015_y, "");
    static_assert(year{2016} >= 2015_y, "");

    auto y = year{2014};
    assert(++y == year{2015});
    assert(y == year{2015});
    assert(y++ == year{2015});
    assert(y == year{2016});
    assert(--y == year{2015});
    assert(y == year{2015});
    assert(y-- == year{2015});
    assert(y == year{2014});

    static_assert(year::min().ok(), "");
    static_assert(year{2015}.ok(), "");
    static_assert(year{2016}.ok(), "");
    static_assert(year::max().ok(), "");

    static_assert(2015_y - 2010_y == years{5}, "");
    static_assert(2015_y - years{5} == 2010_y, "");
    static_assert(2015_y == years{5} + 2010_y, "");
    static_assert(2015_y == 2010_y + years{5}, "");

    y = 2015_y;
    std::ostringstream os;
    os << y;
    assert(os.str() == "2015");
}
