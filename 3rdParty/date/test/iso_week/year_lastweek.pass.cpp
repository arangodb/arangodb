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

// class year_lastweek
// {
//     iso_week::year y_;
//
// public:
//     explicit constexpr year_lastweek(const iso_week::year& y) noexcept;
//
//     constexpr iso_week::year    year()    const noexcept;
//     constexpr iso_week::weeknum weeknum() const noexcept;
//
//     year_lastweek& operator+=(const years& dy) noexcept;
//     year_lastweek& operator-=(const years& dy) noexcept;
//
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const year_lastweek& x, const year_lastweek& y) noexcept;
// constexpr bool operator!=(const year_lastweek& x, const year_lastweek& y) noexcept;
// constexpr bool operator< (const year_lastweek& x, const year_lastweek& y) noexcept;
// constexpr bool operator> (const year_lastweek& x, const year_lastweek& y) noexcept;
// constexpr bool operator<=(const year_lastweek& x, const year_lastweek& y) noexcept;
// constexpr bool operator>=(const year_lastweek& x, const year_lastweek& y) noexcept;
//
// constexpr year_lastweek operator+(const year_lastweek& ym, const years& dy) noexcept;
// constexpr year_lastweek operator+(const years& dy, const year_lastweek& ym) noexcept;
// constexpr year_lastweek operator-(const year_lastweek& ym, const years& dy) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const year_lastweek& ym);

#include "iso_week.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<iso_week::year_lastweek>{}, "");
static_assert(!std::is_default_constructible<iso_week::year_lastweek>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::year_lastweek>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::year_lastweek>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::year_lastweek>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::year_lastweek>{}, "");

static_assert(std::is_trivially_copyable<iso_week::year_lastweek>{}, "");
static_assert(std::is_standard_layout<iso_week::year_lastweek>{}, "");
static_assert(std::is_literal_type<iso_week::year_lastweek>{}, "");

static_assert(std::is_nothrow_constructible<iso_week::year_lastweek,
                                                iso_week::year>{}, "");
static_assert(!std::is_convertible<iso_week::year, iso_week::year_lastweek>{}, "");

int
main()
{
    using namespace iso_week;

    constexpr auto y_wn = year_lastweek{2015_y};
    static_assert(y_wn.year() == 2015_y, "");
    static_assert(y_wn.weeknum() == 53_w, "");
    static_assert(year_lastweek{2016_y}.weeknum() == 52_w, "");

    static_assert(y_wn.ok(), "");

    static_assert(y_wn == year_lastweek{2015_y}, "");
    static_assert(!(y_wn != year_lastweek{2015_y}), "");
    static_assert(y_wn < year_lastweek{2016_y}, "");

    auto y_wn2 = y_wn + years{2};
    assert(y_wn2 == year_lastweek{2017_y});
    y_wn2 -= years{1};
    assert(y_wn2 == year_lastweek{2016_y});

    std::ostringstream os;
    os << y_wn;
    assert(os.str() == "2015-W last");
}
