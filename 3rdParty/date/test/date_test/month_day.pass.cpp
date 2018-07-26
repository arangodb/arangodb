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

// class month_day
// {
// public:
//     constexpr month_day(const date::month& m, const date::day& d) noexcept;
//
//     constexpr date::month month() const noexcept;
//     constexpr date::day   day() const noexcept;
//
//     constexpr bool ok() const noexcept;
// };

// constexpr bool operator==(const month_day& x, const month_day& y) noexcept;
// constexpr bool operator!=(const month_day& x, const month_day& y) noexcept;
// constexpr bool operator< (const month_day& x, const month_day& y) noexcept;
// constexpr bool operator> (const month_day& x, const month_day& y) noexcept;
// constexpr bool operator<=(const month_day& x, const month_day& y) noexcept;
// constexpr bool operator>=(const month_day& x, const month_day& y) noexcept;

// std::ostream& operator<<(std::ostream& os, const month_day& md);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::month_day>{}, "");
static_assert( std::is_default_constructible<date::month_day>{}, "");
static_assert( std::is_trivially_copy_constructible<date::month_day>{}, "");
static_assert( std::is_trivially_copy_assignable<date::month_day>{}, "");
static_assert( std::is_trivially_move_constructible<date::month_day>{}, "");
static_assert( std::is_trivially_move_assignable<date::month_day>{}, "");

static_assert(std::is_nothrow_constructible<date::month_day, date::month,
                                                             date::day>{}, "");

int
main()
{
    using namespace date;

    constexpr month_day md1 = {feb, day{28}};
    constexpr month_day md2 = {mar, day{1}};
#if __cplusplus >= 201402
    static_assert(md1.ok(), "");
    static_assert(md2.ok(), "");
    static_assert(!month_day{feb, day{32}}.ok(), "");
    static_assert(!month_day{month{0}, day{1}}.ok(), "");
#endif
    static_assert(md1.month() == feb, "");
    static_assert(md1.day() == day{28}, "");
    static_assert(md2.month() == mar, "");
    static_assert(md2.day() == day{1}, "");
    static_assert(md1 == md1, "");
    static_assert(md1 != md2, "");
    static_assert(md1 < md2, "");
    std::ostringstream os;
    os << md1;
    assert(os.str() == "Feb/28");
}
