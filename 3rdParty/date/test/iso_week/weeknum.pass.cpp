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

// class weeknum
// {
//     unsigned char wn_;
//
// public:
//     explicit constexpr weeknum(unsigned wn) noexcept;
//
//     weeknum& operator++()    noexcept;
//     weeknum  operator++(int) noexcept;
//     weeknum& operator--()    noexcept;
//     weeknum  operator--(int) noexcept;
//
//     weeknum& operator+=(const weeks& y) noexcept;
//     weeknum& operator-=(const weeks& y) noexcept;
//
//     constexpr explicit operator unsigned() const noexcept;
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const weeknum& x, const weeknum& y) noexcept;
// constexpr bool operator!=(const weeknum& x, const weeknum& y) noexcept;
// constexpr bool operator< (const weeknum& x, const weeknum& y) noexcept;
// constexpr bool operator> (const weeknum& x, const weeknum& y) noexcept;
// constexpr bool operator<=(const weeknum& x, const weeknum& y) noexcept;
// constexpr bool operator>=(const weeknum& x, const weeknum& y) noexcept;
//
// constexpr weeknum  operator+(const weeknum& x, const weeks&   y) noexcept;
// constexpr weeknum  operator+(const weeks&   x, const weeknum& y) noexcept;
// constexpr weeknum  operator-(const weeknum& x, const weeks&   y) noexcept;
// constexpr weeks    operator-(const weeknum& x, const weeknum& y) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const weeknum& wn);
//
// constexpr weeknum operator "" _w(unsigned long long wn) noexcept;

#include "iso_week.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<iso_week::weeknum>{}, "");
static_assert(!std::is_default_constructible<iso_week::weeknum>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::weeknum>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::weeknum>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::weeknum>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::weeknum>{}, "");

static_assert(std::is_trivially_copyable<iso_week::weeknum>{}, "");
static_assert(std::is_standard_layout<iso_week::weeknum>{}, "");
static_assert(std::is_literal_type<iso_week::weeknum>{}, "");

static_assert( std::is_nothrow_constructible<iso_week::weeknum, unsigned>{}, "");
static_assert(!std::is_convertible<unsigned, iso_week::weeknum>{}, "");
static_assert( std::is_nothrow_constructible<unsigned, iso_week::weeknum>{}, "");
static_assert(!std::is_convertible<iso_week::weeknum, unsigned>{}, "");
static_assert(static_cast<unsigned>(iso_week::weeknum{3}) == 3, "");

int
main()
{
    using namespace iso_week;
    using namespace std::chrono;

    static_assert(weeknum{2} == 2_w, "");
    static_assert(unsigned{weeknum{2}} == 2, "");
    static_assert(unsigned{weeknum{3}} == 3, "");
    static_assert(weeknum{2} != 3_w, "");
    static_assert(weeknum{2} < 3_w, "");
    static_assert(weeknum{3} > 2_w, "");
    static_assert(weeknum{2} <= 2_w, "");
    static_assert(weeknum{3} >= 2_w, "");

    auto wn = weeknum{4};
    assert(++wn == weeknum{5});
    assert(wn == weeknum{5});
    assert(wn++ == weeknum{5});
    assert(wn == weeknum{6});
    assert(--wn == weeknum{5});
    assert(wn == weeknum{5});
    assert(wn-- == weeknum{5});
    assert(wn == weeknum{4});

    static_assert(!weeknum{0}.ok(), "");
    static_assert( weeknum{1}.ok(), "");
    static_assert( weeknum{53}.ok(), "");
    static_assert(!weeknum{54}.ok(), "");

    static_assert(15_w - 10_w == weeks{5}, "");
    static_assert(15_w - weeks{5} == 10_w, "");
    static_assert(15_w == weeks{5} + 10_w, "");
    static_assert(15_w == 10_w + weeks{5}, "");
    static_assert(10_w - 15_w == -weeks{5}, "");

    auto w = 5_w;
    std::ostringstream os;
    os << w;
    assert(os.str() == "W05");
}
