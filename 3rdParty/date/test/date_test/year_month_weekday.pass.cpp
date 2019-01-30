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

// class year_month_weekday
// {
// public:
//     year_month_weekday() = default;
//     constexpr year_month_weekday(const date::year& y, const date::month& m,
//                                    const date::weekday_indexed& wdi) noexcept;
//     constexpr year_month_weekday(const sys_days& dp) noexcept;
//
//     year_month_weekday& operator+=(const months& m) noexcept;
//     year_month_weekday& operator-=(const months& m) noexcept;
//     year_month_weekday& operator+=(const years& y)  noexcept;
//     year_month_weekday& operator-=(const years& y)  noexcept;
//
//     constexpr date::year year() const noexcept;
//     constexpr date::month month() const noexcept;
//     constexpr date::weekday weekday() const noexcept;
//     constexpr unsigned index() const noexcept;
//     constexpr date::weekday_indexed weekday_indexed() const noexcept;
//
//     constexpr operator sys_days() const noexcept;
//     constexpr bool ok() const noexcept;
//
// private:
//     static constexpr year_month_weekday from_day_point(const sys_days& dp) noexcept;
// };

// constexpr
//     bool operator==(const year_month_weekday& x, const year_month_weekday& y) noexcept;
// constexpr
//     bool operator!=(const year_month_weekday& x, const year_month_weekday& y) noexcept;

// constexpr
// year_month_weekday
// operator+(const year_month_weekday& ymwd, const months& dm) noexcept;
//
// constexpr
// year_month_weekday
// operator+(const months& dm, const year_month_weekday& ymwd) noexcept;
//
// constexpr
// year_month_weekday
// operator+(const year_month_weekday& ymwd, const years& dy) noexcept;
//
// constexpr
// year_month_weekday
// operator+(const years& dy, const year_month_weekday& ymwd) noexcept;
//
// constexpr
// year_month_weekday
// operator-(const year_month_weekday& ymwd, const months& dm) noexcept;
//
// constexpr
// year_month_weekday
// operator-(const year_month_weekday& ymwd, const years& dy) noexcept;

// std::ostream& operator<<(std::ostream& os, const year_month_weekday& ymwdi);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::year_month_weekday>{}, "");
static_assert( std::is_default_constructible<date::year_month_weekday>{}, "");
static_assert( std::is_trivially_copy_constructible<date::year_month_weekday>{}, "");
static_assert( std::is_trivially_copy_assignable<date::year_month_weekday>{}, "");
static_assert( std::is_trivially_move_constructible<date::year_month_weekday>{}, "");
static_assert( std::is_trivially_move_assignable<date::year_month_weekday>{}, "");

static_assert(std::is_nothrow_constructible<date::year_month_weekday,
                                                date::year, date::month,
                                                date::weekday_indexed>{}, "");
static_assert(std::is_nothrow_constructible<date::year_month_weekday,
                                                date::sys_days>{}, "");
static_assert(std::is_convertible<date::sys_days, date::year_month_weekday>{}, "");
static_assert(std::is_nothrow_constructible<date::sys_days,
                                                date::year_month_weekday>{}, "");
static_assert(std::is_convertible<date::year_month_weekday, date::sys_days>{}, "");

void
test_arithmetic()
{
    using namespace date;

    for (int y1 = 2010; y1 <= 2015; ++y1)
    {
        for (int m1 = 1; m1 <= 12; ++m1)
        {
            auto ymd1 = mon[2]/m1/y1;;
            auto ymd2 = ymd1 + months(24);
            assert(ymd2 == mon[2]/m1/(y1+2));
            ymd2 = ymd1 - months(24);
            assert(ymd2 == mon[2]/m1/(y1-2));
            for (int m2 = -24; m2 <= 24; ++m2)
            {
                months m{m2};
                auto ymd3 = ymd1 + m;
                months dm = year_month{ymd3.year(), ymd3.month()} -
                            year_month{ymd2.year(), ymd2.month()};
                assert(dm == m + years{2});
                assert(ymd3 - m == ymd1);
                assert(ymd3 + -m == ymd1);
                assert(-m + ymd3 == ymd1);
                assert((year_month_weekday{ymd1} += m) == ymd3);
                assert((year_month_weekday{ymd3} -= m) == ymd1);
            }
            for (int y2 = -2; y2 <= 5; ++y2)
            {
                years y{y2};
                auto ymd3 = ymd1 + y;
                years dy = floor<years>(year_month{ymd3.year(), ymd3.month()} -
                                        year_month{ymd2.year(), ymd2.month()});
                assert(dy == y + years{2});
                assert(ymd3 - y == ymd1);
                assert(ymd3 + -y == ymd1);
                assert(-y + ymd3 == ymd1);
                assert((year_month_weekday{ymd1} += y) == ymd3);
                assert((year_month_weekday{ymd3} -= y) == ymd1);
            }
        }
    }
}

int
main()
{
    using namespace date;

    constexpr year_month_weekday ymdl1 = {2015_y, aug, weekday_indexed{fri, 2}};
#if __cplusplus >= 201402
    static_assert(ymdl1.ok(), "");
#endif
    static_assert(ymdl1.year() == 2015_y, "");
    static_assert(ymdl1.month() == aug, "");
    static_assert(ymdl1.weekday() == fri, "");
    static_assert(ymdl1.index() == 2u, "");
    static_assert(ymdl1.weekday_indexed() == fri[2], "");
#if __cplusplus >= 201402
    constexpr sys_days dp = ymdl1;
    constexpr year_month_day ymd = dp;
    static_assert(ymd == 2015_y/aug/14, "");
#endif

    constexpr year_month_weekday ymdl2 = sat[1]/aug/2015;

    static_assert(ymdl1 == ymdl1, "");
    static_assert(ymdl1 != ymdl2, "");

    test_arithmetic();

    std::ostringstream os;
    os << ymdl1;
    assert(os.str() == "2015/Aug/Fri[2]");

}
