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

// class month_weekday_last
// {
// public:
//     constexpr month_weekday_last(const date::month& m,
//                                  const date::weekday_last& wdl) noexcept;
//
//     constexpr date::month        month()        const noexcept;
//     constexpr date::weekday_last weekday_last() const noexcept;
//
//     constexpr bool ok() const noexcept;
// };

// constexpr
//     bool operator==(const month_weekday_last& x, const month_weekday_last& y) noexcept;
// constexpr
//     bool operator!=(const month_weekday_last& x, const month_weekday_last& y) noexcept;

// std::ostream& operator<<(std::ostream& os, const month_weekday_last& mwdl);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::month_weekday_last>{}, "");
static_assert(!std::is_default_constructible<date::month_weekday_last>{}, "");
static_assert( std::is_trivially_copy_constructible<date::month_weekday_last>{}, "");
static_assert( std::is_trivially_copy_assignable<date::month_weekday_last>{}, "");
static_assert( std::is_trivially_move_constructible<date::month_weekday_last>{}, "");
static_assert( std::is_trivially_move_assignable<date::month_weekday_last>{}, "");

static_assert(std::is_nothrow_constructible<date::month_weekday_last,
                                                date::month, date::weekday_last>{}, "");

int
main()
{
    using namespace date;

    constexpr month_weekday_last mwdl1 = {feb, sat[last]};
    constexpr month_weekday_last mwdl2 = {mar, mon[last]};
    static_assert(mwdl1.ok(), "");
    static_assert(mwdl2.ok(), "");
    static_assert(!month_weekday_last{month{0}, sun[last]}.ok(), "");
    static_assert(mwdl1.month() == feb, "");
    static_assert(mwdl1.weekday_last() == sat[last], "");
    static_assert(mwdl2.month() == mar, "");
    static_assert(mwdl2.weekday_last() == mon[last], "");
    static_assert(mwdl1 == mwdl1, "");
    static_assert(mwdl1 != mwdl2, "");
    std::ostringstream os;
    os << mwdl1;
    assert(os.str() == "Feb/Sat[last]");
}
