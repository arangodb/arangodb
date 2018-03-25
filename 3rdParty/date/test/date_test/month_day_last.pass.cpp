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

// class month_day_last
// {
// public:
//     constexpr explicit month_day_last(const date::month& m) noexcept;
//
//     constexpr date::month month() const noexcept;
//     constexpr bool ok() const noexcept;
// };

// constexpr bool operator==(const month_day_last& x, const month_day_last& y) noexcept;
// constexpr bool operator!=(const month_day_last& x, const month_day_last& y) noexcept;
// constexpr bool operator< (const month_day_last& x, const month_day_last& y) noexcept;
// constexpr bool operator> (const month_day_last& x, const month_day_last& y) noexcept;
// constexpr bool operator<=(const month_day_last& x, const month_day_last& y) noexcept;
// constexpr bool operator>=(const month_day_last& x, const month_day_last& y) noexcept;

// std::ostream& operator<<(std::ostream& os, const month_day_last& mdl);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::month_day_last>{}, "");
static_assert(!std::is_default_constructible<date::month_day_last>{}, "");
static_assert( std::is_trivially_copy_constructible<date::month_day_last>{}, "");
static_assert( std::is_trivially_copy_assignable<date::month_day_last>{}, "");
static_assert( std::is_trivially_move_constructible<date::month_day_last>{}, "");
static_assert( std::is_trivially_move_assignable<date::month_day_last>{}, "");

static_assert(std::is_nothrow_constructible<date::month_day_last, date::month>{}, "");
static_assert(!std::is_convertible<date::month, date::month_day_last>{}, "");

int
main()
{
    using namespace date;

    constexpr month_day_last mdl1{feb};
    constexpr month_day_last mdl2{mar};
    static_assert(mdl1.ok(), "");
    static_assert(mdl2.ok(), "");
    static_assert(!month_day_last{month{0}}.ok(), "");
    static_assert(mdl1.month() == feb, "");
    static_assert(mdl2.month() == mar, "");
    static_assert(mdl1 == mdl1, "");
    static_assert(mdl1 != mdl2, "");
    static_assert(mdl1 < mdl2, "");
    std::ostringstream os;
    os << mdl1;
    assert(os.str() == "Feb/last");
}
