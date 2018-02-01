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

// class year_weeknum_weekday

// class year_weeknum_weekday
// {
// public:
//     constexpr year_weeknum_weekday(const iso_week::year& y, const iso_week::weeknum& wn,
//                                    const iso_week::weekday& wd) noexcept;
//     constexpr year_weeknum_weekday(const year_lastweek_weekday& ylwwd) noexcept;
//     constexpr year_weeknum_weekday(const sys_days& dp) noexcept;
//
//     year_weeknum_weekday& operator+=(const years& y) noexcept;
//     year_weeknum_weekday& operator-=(const years& y) noexcept;
//
//     constexpr iso_week::year    year()    const noexcept;
//     constexpr iso_week::weeknum weeknum() const noexcept;
//     constexpr iso_week::weekday weekday() const noexcept;
//
//     constexpr operator sys_days() const noexcept;
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const year_weeknum_weekday& x, const year_weeknum_weekday& y) noexcept;
// constexpr bool operator!=(const year_weeknum_weekday& x, const year_weeknum_weekday& y) noexcept;
// constexpr bool operator< (const year_weeknum_weekday& x, const year_weeknum_weekday& y) noexcept;
// constexpr bool operator> (const year_weeknum_weekday& x, const year_weeknum_weekday& y) noexcept;
// constexpr bool operator<=(const year_weeknum_weekday& x, const year_weeknum_weekday& y) noexcept;
// constexpr bool operator>=(const year_weeknum_weekday& x, const year_weeknum_weekday& y) noexcept;
//
// constexpr year_weeknum_weekday operator+(const year_weeknum_weekday& ywnwd, const years& y) noexcept;
// constexpr year_weeknum_weekday operator+(const years& y, const year_weeknum_weekday& ywnwd) noexcept;
// constexpr year_weeknum_weekday operator-(const year_weeknum_weekday& ywnwd, const years& y) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const year_weeknum_weekday& ywnwd);

#include "iso_week.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<iso_week::year_weeknum_weekday>{}, "");
static_assert(!std::is_default_constructible<iso_week::year_weeknum_weekday>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::year_weeknum_weekday>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::year_weeknum_weekday>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::year_weeknum_weekday>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::year_weeknum_weekday>{}, "");

static_assert(std::is_trivially_copyable<iso_week::year_weeknum_weekday>{}, "");
static_assert(std::is_standard_layout<iso_week::year_weeknum_weekday>{}, "");
static_assert(std::is_literal_type<iso_week::year_weeknum_weekday>{}, "");

static_assert( std::is_nothrow_constructible<iso_week::year_weeknum_weekday,
                                                 iso_week::year, iso_week::weeknum,
                                                 iso_week::weekday>{}, "");
static_assert( std::is_nothrow_constructible<iso_week::year_weeknum_weekday,
                                                 iso_week::year_lastweek_weekday>{}, "");
static_assert( std::is_convertible<iso_week::year_lastweek_weekday,
                                       iso_week::year_weeknum_weekday>{}, "");
static_assert( std::is_nothrow_constructible<iso_week::year_weeknum_weekday,
                                                 iso_week::sys_days>{}, "");
static_assert( std::is_convertible<iso_week::sys_days,
                                       iso_week::year_weeknum_weekday>{}, "");
static_assert( std::is_nothrow_constructible<iso_week::sys_days,
                                                 iso_week::year_weeknum_weekday>{}, "");
static_assert( std::is_convertible<iso_week::year_weeknum_weekday,
                                       iso_week::sys_days>{}, "");

int
main()
{
    using namespace iso_week;

    constexpr auto x0 = year_weeknum_weekday{2015_y, 52_w, tue};
    static_assert(x0.year() == 2015_y, "");
    static_assert(x0.weeknum() == 52_w, "");
    static_assert(x0.weekday() == tue, "");

    constexpr auto x1 = year_weeknum_weekday{2015_y/last/tue};
    static_assert(x1.year() == 2015_y, "");
    static_assert(x1.weeknum() == 53_w, "");
    static_assert(x1.weekday() == tue, "");

    constexpr year_weeknum_weekday x2 = sys_days{days{16792}};
    static_assert(x2.year() == 2015_y, "");
    static_assert(x2.weeknum() == 52_w, "");
    static_assert(x2.weekday() == wed, "");

    auto x3 = x2;
    x3 += years{2};
    assert(x3.year() == 2017_y);
    assert(x3.weeknum() == 52_w);
    assert(x3.weekday() == wed);

    x3 -= years{1};
    assert(x3.year() == 2016_y);
    assert(x3.weeknum() == 52_w);
    assert(x3.weekday() == wed);

    constexpr sys_days dp = 2015_y/52_w/wed;
    static_assert(dp == sys_days{days{16792}}, "");

    static_assert(x0.ok(), "");
    static_assert(x1.ok(), "");
    static_assert(x2.ok(), "");
    assert(x3.ok());
    static_assert(!(2016_y/53_w/sun).ok(), "");

    static_assert(2015_y/52_w/mon < 2015_y/52_w/sun, "");

    static_assert(x0 + years{3} == 2018_y/52_w/tue, "");
    static_assert(years{3} + x0 == 2018_y/52_w/tue, "");
    static_assert(x0 - years{3} == 2012_y/52_w/tue, "");

    std::ostringstream os;
    os << x0;
    assert(os.str() == "2015-W52-Tue");
}
