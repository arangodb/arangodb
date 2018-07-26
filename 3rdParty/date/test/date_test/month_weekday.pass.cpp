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

// class month_weekday
// {
// public:
//     constexpr month_weekday(const date::month& m,
//                               const date::weekday_indexed& wdi) noexcept;
//
//     constexpr date::month           month()           const noexcept;
//     constexpr date::weekday_indexed weekday_indexed() const noexcept;
//
//     constexpr bool ok() const noexcept;
// };

// constexpr bool operator==(const month_weekday& x, const month_weekday& y) noexcept;
// constexpr bool operator!=(const month_weekday& x, const month_weekday& y) noexcept;

// std::ostream& operator<<(std::ostream& os, const month_weekday& mwd);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::month_weekday>{}, "");
static_assert(!std::is_default_constructible<date::month_weekday>{}, "");
static_assert( std::is_trivially_copy_constructible<date::month_weekday>{}, "");
static_assert( std::is_trivially_copy_assignable<date::month_weekday>{}, "");
static_assert( std::is_trivially_move_constructible<date::month_weekday>{}, "");
static_assert( std::is_trivially_move_assignable<date::month_weekday>{}, "");

static_assert(std::is_nothrow_constructible<date::month_weekday,
                                                date::month,
                                                date::weekday_indexed>{}, "");

int
main()
{
    using namespace date;

    constexpr month_weekday mwd1 = {feb, sat[4]};
    constexpr month_weekday mwd2 = {mar, mon[1]};
    static_assert(mwd1.ok(), "");
    static_assert(mwd2.ok(), "");
    static_assert(!month_weekday{feb, sat[0]}.ok(), "");
    static_assert(!month_weekday{month{0}, sun[1]}.ok(), "");
    static_assert(mwd1.month() == feb, "");
    static_assert(mwd1.weekday_indexed() == sat[4], "");
    static_assert(mwd2.month() == mar, "");
    static_assert(mwd2.weekday_indexed() == mon[1], "");
    static_assert(mwd1 == mwd1, "");
    static_assert(mwd1 != mwd2, "");
    std::ostringstream os;
    os << mwd1;
    assert(os.str() == "Feb/Sat[4]");
}
