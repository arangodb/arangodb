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

// class year_month_day
// {
// public:
//     constexpr year_month_day(const date::year& y, const date::month& m,
//                                const date::day& d) noexcept;
//     constexpr year_month_day(const year_month_day_last& ymdl) noexcept;
//     constexpr year_month_day(const sys_days& dp) noexcept;
//
//     year_month_day& operator+=(const months& m) noexcept;
//     year_month_day& operator-=(const months& m) noexcept;
//     year_month_day& operator+=(const years& y)  noexcept;
//     year_month_day& operator-=(const years& y)  noexcept;
//
//     constexpr date::year  year()  const noexcept;
//     constexpr date::month month() const noexcept;
//     constexpr date::day   day()   const noexcept;
//
//     constexpr operator sys_days() const noexcept;
//     constexpr bool ok() const noexcept;
// };

// constexpr bool operator==(const year_month_day& x, const year_month_day& y) noexcept;
// constexpr bool operator!=(const year_month_day& x, const year_month_day& y) noexcept;
// constexpr bool operator< (const year_month_day& x, const year_month_day& y) noexcept;
// constexpr bool operator> (const year_month_day& x, const year_month_day& y) noexcept;
// constexpr bool operator<=(const year_month_day& x, const year_month_day& y) noexcept;
// constexpr bool operator>=(const year_month_day& x, const year_month_day& y) noexcept;

// constexpr year_month_day operator+(const year_month_day& ymd, const months& dm) noexcept;
// constexpr year_month_day operator+(const months& dm, const year_month_day& ymd) noexcept;
// constexpr year_month_day operator-(const year_month_day& ymd, const months& dm) noexcept;
// constexpr year_month_day operator+(const year_month_day& ymd, const years& dy)  noexcept;
// constexpr year_month_day operator+(const years& dy, const year_month_day& ymd)  noexcept;
// constexpr year_month_day operator-(const year_month_day& ymd, const years& dy)  noexcept;

// std::ostream& operator<<(std::ostream& os, const year_month_day& ymd);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::year_month_day>{}, "");
static_assert( std::is_default_constructible<date::year_month_day>{}, "");
static_assert( std::is_trivially_copy_constructible<date::year_month_day>{}, "");
static_assert( std::is_trivially_copy_assignable<date::year_month_day>{}, "");
static_assert( std::is_trivially_move_constructible<date::year_month_day>{}, "");
static_assert( std::is_trivially_move_assignable<date::year_month_day>{}, "");

static_assert(std::is_nothrow_constructible<date::year_month_day, date::year,
                                                                  date::month,
                                                                  date::day>{}, "");
static_assert(std::is_nothrow_constructible<date::year_month_day,
                                            date::year_month_day_last>{}, "");
static_assert(std::is_convertible<date::year_month_day_last, date::year_month_day>{}, "");
static_assert(std::is_nothrow_constructible<date::year_month_day, date::sys_days>{}, "");
static_assert(std::is_convertible<date::sys_days, date::year_month_day>{}, "");
static_assert(std::is_nothrow_constructible<date::sys_days, date::year_month_day>{}, "");
static_assert(std::is_convertible<date::year_month_day, date::sys_days>{}, "");

void
test_arithmetic()
{
    using namespace date;

    for (int y1 = 2010; y1 <= 2015; ++y1)
    {
        for (unsigned m1 = 1; m1 <= 12; ++m1)
        {
            year_month_day ymd1{year{y1}, month{m1}, 9_d};
            year_month_day ymd2 = ymd1 + months(24);
            assert((ymd2 == year_month_day{year{y1+2}, ymd1.month(), ymd1.day()}));
            ymd2 = ymd1 - months(24);
            assert((ymd2 == year_month_day{year{y1-2}, ymd1.month(), ymd1.day()}));
            for (int m2 = -24; m2 <= 24; ++m2)
            {
                months m{m2};
                year_month_day ymd3 = ymd1 + m;
                months dm = year_month{ymd3.year(), ymd3.month()} -
                            year_month{ymd2.year(), ymd2.month()};
                assert(dm == m + years{2});
                assert(ymd3 - m == ymd1);
                assert(ymd3 + -m == ymd1);
                assert(-m + ymd3 == ymd1);
                assert((year_month_day{ymd1} += m) == ymd3);
                assert((year_month_day{ymd3} -= m) == ymd1);
            }
            for (int y2 = -2; y2 <= 5; ++y2)
            {
                years y{y2};
                year_month_day ymd3 = ymd1 + y;
                years dy = floor<years>(year_month{ymd3.year(), ymd3.month()} -
                                        year_month{ymd2.year(), ymd2.month()});
                assert(dy == y + years{2});
                assert(ymd3 - y == ymd1);
                assert(ymd3 + -y == ymd1);
                assert(-y + ymd3 == ymd1);
                assert((year_month_day{ymd1} += y) == ymd3);
                assert((year_month_day{ymd3} -= y) == ymd1);
            }
        }
    }
}

void
test_day_point_conversion()
{
    using namespace date;
    year y   = year{-1000};
    year end =       3000_y;
    sys_days prev_dp = sys_days(year_month_day{y, jan, 1_d}) - days{1};
    weekday   prev_wd = weekday{prev_dp};
    for (; y <= end; ++y)
    {
        month m = jan;
        do
        {
            day last_day = year_month_day_last{y, month_day_last{m}}.day();
            for (day d = 1_d; d <= last_day; ++d)
            {
                year_month_day ymd = {y, m, d};
                assert(ymd.ok());
                sys_days dp = ymd;
                assert(dp == prev_dp + days{1});
                year_month_day ymd2 = dp;
                assert(ymd2 == ymd);
                weekday wd = dp;
                assert(wd.ok());
                assert(wd == prev_wd + days{1});
                prev_wd = wd;
                prev_dp = dp;
            }
        } while (++m != jan);
    }
}

int
main()
{
    using namespace date;

    constexpr year_month_day ymd1 = {2015_y, aug, 9_d};
#if __cplusplus >= 201402
    static_assert(ymd1.ok(), "");
#endif
    static_assert(ymd1.year() == 2015_y, "");
    static_assert(ymd1.month() == aug, "");
    static_assert(ymd1.day() == 9_d, "");

#if __cplusplus >= 201402
    constexpr sys_days dp = ymd1;
    static_assert(dp.time_since_epoch() == days{16656}, "");
    constexpr year_month_day ymd2 = dp;
    static_assert(ymd1 == ymd2, "");
#endif

    constexpr year_month_day ymd3 = {1969_y, dec, 31_d};
#if __cplusplus >= 201402
    static_assert(ymd3.ok(), "");
#endif
    static_assert(ymd3.year() == 1969_y, "");
    static_assert(ymd3.month() == dec, "");
    static_assert(ymd3.day() == 31_d, "");

#if __cplusplus >= 201402
    constexpr sys_days dp3 = ymd3;
    static_assert(dp3.time_since_epoch() == days{-1}, "");
    constexpr year_month_day ymd4 = dp3;
    static_assert(ymd3 == ymd4, "");
#endif

    static_assert(ymd1 != ymd3, "");
    static_assert(ymd1 > ymd3, "");
    static_assert(ymd3 < ymd1, "");
    static_assert(ymd1 >= ymd1, "");
    static_assert(ymd3 <= ymd1, "");

    test_arithmetic();
    test_day_point_conversion();

    std::ostringstream os;
    os << ymd1;
    assert(os.str() == "2015-08-09");

#if __cplusplus >= 201402
    static_assert( (2000_y/feb/29).ok(), "");
    static_assert(!(2000_y/feb/30).ok(), "");
    static_assert( (2100_y/feb/28).ok(), "");
    static_assert(!(2100_y/feb/29).ok(), "");

    static_assert(sys_days(2100_y/feb/28) + days{1} == sys_days(2100_y/mar/1), "");
    static_assert(sys_days(2000_y/mar/1) - sys_days(2000_y/feb/28) == days{2}, "");
    static_assert(sys_days(2100_y/mar/1) - sys_days(2100_y/feb/28) == days{1}, "");

    static_assert(jan/31/2015 == jan/last/2015, "");
    static_assert(feb/28/2015 == feb/last/2015, "");
    static_assert(mar/31/2015 == mar/last/2015, "");
    static_assert(apr/30/2015 == apr/last/2015, "");
    static_assert(may/31/2015 == may/last/2015, "");
    static_assert(jun/30/2015 == jun/last/2015, "");
    static_assert(jul/31/2015 == jul/last/2015, "");
    static_assert(aug/31/2015 == aug/last/2015, "");
    static_assert(sep/30/2015 == sep/last/2015, "");
    static_assert(oct/31/2015 == oct/last/2015, "");
    static_assert(nov/30/2015 == nov/last/2015, "");
    static_assert(dec/31/2015 == dec/last/2015, "");

    static_assert(jan/31/2016 == jan/last/2016, "");
    static_assert(feb/29/2016 == feb/last/2016, "");
    static_assert(mar/31/2016 == mar/last/2016, "");
    static_assert(apr/30/2016 == apr/last/2016, "");
    static_assert(may/31/2016 == may/last/2016, "");
    static_assert(jun/30/2016 == jun/last/2016, "");
    static_assert(jul/31/2016 == jul/last/2016, "");
    static_assert(aug/31/2016 == aug/last/2016, "");
    static_assert(sep/30/2016 == sep/last/2016, "");
    static_assert(oct/31/2016 == oct/last/2016, "");
    static_assert(nov/30/2016 == nov/last/2016, "");
    static_assert(dec/31/2016 == dec/last/2016, "");
#endif
}
