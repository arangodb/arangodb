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

// class day
// {
//     unsigned char d_;
// public:
//     explicit constexpr day(unsigned d) noexcept;
//
//     day& operator++()    noexcept;
//     day  operator++(int) noexcept;
//     day& operator--()    noexcept;
//     day  operator--(int) noexcept;
//
//     day& operator+=(const days& d) noexcept;
//     day& operator-=(const days& d) noexcept;
//
//     constexpr explicit operator unsigned() const noexcept;
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const day& x, const day& y) noexcept;
// constexpr bool operator!=(const day& x, const day& y) noexcept;
// constexpr bool operator< (const day& x, const day& y) noexcept;
// constexpr bool operator> (const day& x, const day& y) noexcept;
// constexpr bool operator<=(const day& x, const day& y) noexcept;
// constexpr bool operator>=(const day& x, const day& y) noexcept;
//
// constexpr day  operator+(const day&  x, const days& y) noexcept;
// constexpr day  operator+(const days& x, const day&  y) noexcept;
// constexpr day  operator-(const day&  x, const days& y) noexcept;
// constexpr days operator-(const day&  x, const day&  y) noexcept;
//
// constexpr day operator "" _d(unsigned long long d) noexcept;
// std::ostream& operator<<(std::ostream& os, const day& d);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::day>{}, "");
static_assert( std::is_default_constructible<date::day>{}, "");
static_assert( std::is_trivially_copy_constructible<date::day>{}, "");
static_assert( std::is_trivially_copy_assignable<date::day>{}, "");
static_assert( std::is_trivially_move_constructible<date::day>{}, "");
static_assert( std::is_trivially_move_assignable<date::day>{}, "");

static_assert( std::is_nothrow_constructible<date::day, unsigned>{}, "");
static_assert( std::is_nothrow_constructible<unsigned, date::day>{}, "");
static_assert(!std::is_convertible<unsigned, date::day>{}, "");
static_assert(!std::is_convertible<date::day, unsigned>{}, "");
static_assert(static_cast<unsigned>(date::day{1}) == 1, "");

static_assert(!date::day{0}.ok(), "");
static_assert( date::day{1}.ok(), "");
static_assert( date::day{2}.ok(), "");
static_assert( date::day{3}.ok(), "");
static_assert( date::day{29}.ok(), "");
static_assert( date::day{30}.ok(), "");
static_assert( date::day{31}.ok(), "");
static_assert(!date::day{32}.ok(), "");

int
main()
{
    using namespace date;
    static_assert(std::is_same<decltype(1_d), day>{}, "");

    static_assert(1_d == day{1}, "");
    static_assert(2_d == day{2}, "");

    static_assert(  1_d == 1_d, "");
    static_assert(!(1_d == 2_d), "");
    static_assert(!(2_d == 1_d), "");

    static_assert(!(1_d != 1_d), "");
    static_assert(  1_d != 2_d, "");
    static_assert(  2_d != 1_d, "");

    static_assert(!(1_d <  1_d), "");
    static_assert(  1_d <  2_d, "");
    static_assert(!(2_d <  1_d), "");

    static_assert(  1_d <= 1_d, "");
    static_assert(  1_d <= 2_d, "");
    static_assert(!(2_d <= 1_d), "");

    static_assert(!(1_d >  1_d), "");
    static_assert(!(1_d >  2_d), "");
    static_assert(  2_d >  1_d, "");

    static_assert(  1_d >= 1_d, "");
    static_assert(!(1_d >= 2_d), "");
    static_assert(  2_d >= 1_d, "");

    static_assert(3_d + days{7} == 10_d, "");
    static_assert(days{7} + 3_d == 10_d, "");
    static_assert(3_d + weeks{1} == 10_d, "");
    static_assert(weeks{1} + 3_d == 10_d, "");

    static_assert(7_d - days{3} == 4_d, "");
    static_assert(3_d - 7_d == days{-4}, "");
    static_assert(25_d - 11_d == weeks{2}, "");

    auto d = 1_d;
    assert(++d == 2_d);
    assert(d++ == 2_d);
    assert(d == 3_d);
    assert(d-- == 3_d);
    assert(d == 2_d);
    assert(--d == 1_d);
    assert((d += days{2}) == 3_d);
    assert((d -= days{2}) == 1_d);

    std::ostringstream os;
    os << d;
    assert(os.str() == "01");
    d += days{11};
    os.str("");
    os << d;
    assert(os.str() == "12");
}
