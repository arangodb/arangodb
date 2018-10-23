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

// class year_lastweek_weekday
// {
// public:
//     constexpr year_lastweek_weekday(const iso_week::year& y,
//                                     const iso_week::weekday& wd) noexcept;
//
//     year_lastweek_weekday& operator+=(const years& y) noexcept;
//     year_lastweek_weekday& operator-=(const years& y) noexcept;
//
//     constexpr iso_week::year    year()    const noexcept;
//     constexpr iso_week::weeknum weeknum() const noexcept;
//     constexpr iso_week::weekday weekday() const noexcept;
//
//     constexpr operator sys_days() const noexcept;
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const year_lastweek_weekday& x, const year_lastweek_weekday& y) noexcept;
// constexpr bool operator!=(const year_lastweek_weekday& x, const year_lastweek_weekday& y) noexcept;
// constexpr bool operator< (const year_lastweek_weekday& x, const year_lastweek_weekday& y) noexcept;
// constexpr bool operator> (const year_lastweek_weekday& x, const year_lastweek_weekday& y) noexcept;
// constexpr bool operator<=(const year_lastweek_weekday& x, const year_lastweek_weekday& y) noexcept;
// constexpr bool operator>=(const year_lastweek_weekday& x, const year_lastweek_weekday& y) noexcept;
//
// constexpr year_lastweek_weekday operator+(const year_lastweek_weekday& x, const years& y) noexcept;
// constexpr year_lastweek_weekday operator+(const years& y, const year_lastweek_weekday& x) noexcept;
// constexpr year_lastweek_weekday operator-(const year_lastweek_weekday& x, const years& y) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const year_lastweek_weekday& x);

#include "iso_week.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<iso_week::year_lastweek_weekday>{}, "");
static_assert(!std::is_default_constructible<iso_week::year_lastweek_weekday>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::year_lastweek_weekday>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::year_lastweek_weekday>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::year_lastweek_weekday>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::year_lastweek_weekday>{}, "");

static_assert(std::is_trivially_copyable<iso_week::year_lastweek_weekday>{}, "");
static_assert(std::is_standard_layout<iso_week::year_lastweek_weekday>{}, "");
static_assert(std::is_literal_type<iso_week::year_lastweek_weekday>{}, "");

static_assert( std::is_nothrow_constructible<iso_week::year_lastweek_weekday,
                                                 iso_week::year, iso_week::weekday>{}, "");
static_assert( std::is_nothrow_constructible<iso_week::sys_days,
                                                 iso_week::year_lastweek_weekday>{}, "");
static_assert( std::is_convertible<iso_week::year_lastweek_weekday,
                                       iso_week::sys_days>{}, "");

int
main()
{
    using namespace iso_week;

    constexpr auto x0 = year_lastweek_weekday{2015_y, tue};
    static_assert(x0.year() == 2015_y, "");
    static_assert(x0.weeknum() == 53_w, "");
    static_assert(x0.weekday() == tue, "");

    auto x3 = x0;
    x3 += years{2};
    assert(x3.year() == 2017_y);
    assert(x3.weeknum() == 52_w);
    assert(x3.weekday() == tue);

    x3 -= years{1};
    assert(x3.year() == 2016_y);
    assert(x3.weeknum() == 52_w);
    assert(x3.weekday() == tue);

    x3 -= years{1};
    assert(x3.year() == 2015_y);
    assert(x3.weeknum() == 53_w);
    assert(x3.weekday() == tue);

    constexpr sys_days dp = 2015_y/last/wed;
    static_assert(dp == sys_days{days{16799}}, "");

    static_assert(x0.ok(), "");
    assert(x3.ok());

    static_assert(2015_y/last/mon < 2015_y/last/sun, "");

    static_assert(x0 + years{3} == 2018_y/last/tue, "");
    static_assert(years{3} + x0 == 2018_y/last/tue, "");
    static_assert(x0 - years{3} == 2012_y/last/tue, "");

    std::ostringstream os;
    os << x0;
    assert(os.str() == "2015-W last-Tue");

    for (auto y = 1950_y; y <= 2050_y; ++y)
    {
        auto wd = mon;
        do
        {
            auto x = y/last/wd;
            assert(date::year_month_day{x} == date::year_month_day{year_weeknum_weekday{x}});
            ++wd;
        } while (wd != mon);
    }
}
