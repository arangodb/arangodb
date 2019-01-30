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

// class year_month
// {
// public:
//     constexpr year_month(const date::year& y, const date::month& m) noexcept;
//
//     constexpr date::year  year()  const noexcept;
//     constexpr date::month month() const noexcept;
//
//     year_month& operator+=(const months& dm) noexcept;
//     year_month& operator-=(const months& dm) noexcept;
//     year_month& operator+=(const years& dy) noexcept;
//     year_month& operator-=(const years& dy) noexcept;
//
//     constexpr bool ok() const noexcept;
// };

// constexpr bool operator==(const year_month& x, const year_month& y) noexcept;
// constexpr bool operator!=(const year_month& x, const year_month& y) noexcept;
// constexpr bool operator< (const year_month& x, const year_month& y) noexcept;
// constexpr bool operator> (const year_month& x, const year_month& y) noexcept;
// constexpr bool operator<=(const year_month& x, const year_month& y) noexcept;
// constexpr bool operator>=(const year_month& x, const year_month& y) noexcept;

// constexpr year_month operator+(const year_month& ym, const months& dm) noexcept;
// constexpr year_month operator+(const months& dm, const year_month& ym) noexcept;
// constexpr year_month operator-(const year_month& ym, const months& dm) noexcept;

// constexpr months operator-(const year_month& x, const year_month& y) noexcept;
// constexpr year_month operator+(const year_month& ym, const years& dy) noexcept;
// constexpr year_month operator+(const years& dy, const year_month& ym) noexcept;
// constexpr year_month operator-(const year_month& ym, const years& dy) noexcept;

// std::ostream& operator<<(std::ostream& os, const year_month& ym);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::year_month>{}, "");
static_assert( std::is_default_constructible<date::year_month>{}, "");
static_assert( std::is_trivially_copy_constructible<date::year_month>{}, "");
static_assert( std::is_trivially_copy_assignable<date::year_month>{}, "");
static_assert( std::is_trivially_move_constructible<date::year_month>{}, "");
static_assert( std::is_trivially_move_assignable<date::year_month>{}, "");

static_assert(std::is_nothrow_constructible<date::year_month, date::year,
                                                              date::month>{}, "");

void
test_arithmetic()
{
    using namespace date;
    using namespace std::chrono;

    for (int y1 = 2010; y1 <= 2015; ++y1)
    {
        for (int y2 = 2010; y2 <= 2015; ++y2)
        {
            for (unsigned m1 = 1; m1 <= 12; ++m1)
            {
                for (unsigned m2 = 1; m2 <= 12; ++m2)
                {
                    year_month ym1{year{y1}, month{m1}};
                    year_month ym2 = {year{y2}, month{m2}};
                    months dm = ym1 - ym2;
                    assert((dm < months{0}) == (ym1 < ym2));
                    assert((dm == months{0}) == (ym1 == ym2));
                    assert((dm > months{0}) == (ym1 > ym2));
                    assert(dm + ym2 == ym1);
                    assert(ym2 + dm == ym1);
                    assert(ym1 - dm == ym2);
                    years dy{y1-y2};
                    assert((ym1 + dy == year_month{ym1.year() + dy, ym1.month()}));
                    assert((dy + ym1 == year_month{ym1.year() + dy, ym1.month()}));
                    assert((ym1 - dy == year_month{ym1.year() - dy, ym1.month()}));
                    assert((year_month{ym2} += dm) == ym1);
                    assert((year_month{ym1} -= dm) == ym2);
                    assert(((year_month{ym1} += dy) ==
                        year_month{ym1.year() + dy, ym1.month()}));
                    assert(((year_month{ym1} -= dy) ==
                        year_month{ym1.year() - dy, ym1.month()}));
                }
            }
        }
    }
}

int
main()
{
    using namespace date;

    constexpr year_month ym1 = {2015_y, jun};
    static_assert(ym1.year() == year{2015}, "");
    static_assert(ym1.month() == jun, "");
    static_assert(ym1.ok(), "");

    constexpr year_month ym2 = {2016_y, may};
    static_assert(ym2.year() == year{2016}, "");
    static_assert(ym2.month() == may, "");
    static_assert(ym2.ok(), "");

    static_assert(ym1 == ym1, "");
    static_assert(ym1 != ym2, "");
    static_assert(ym1 < ym2, "");
    static_assert(ym1 <= ym2, "");
    static_assert(ym2 > ym1, "");
    static_assert(ym2 >= ym2, "");

    static_assert(ym2 - ym1 == months{11}, "");
    static_assert(ym1 - ym2 == -months{11}, "");

    test_arithmetic();

    std::ostringstream os;
    os << ym1;
    assert(os.str() == "2015/Jun");
}
