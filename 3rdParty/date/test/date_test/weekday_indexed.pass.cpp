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

// class weekday_indexed
// {
// public:
//     weekday_indexed() = default;
//     constexpr weekday_indexed(const date::weekday& wd, unsigned index) noexcept;
//
//     constexpr date::weekday weekday() const noexcept;
//     constexpr unsigned index() const noexcept;
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const weekday_indexed& x, const weekday_indexed& y) noexcept;
// constexpr bool operator!=(const weekday_indexed& x, const weekday_indexed& y) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const weekday_indexed& wdi);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::weekday_indexed>{}, "");
static_assert( std::is_default_constructible<date::weekday_indexed>{}, "");
static_assert( std::is_trivially_copy_constructible<date::weekday_indexed>{}, "");
static_assert( std::is_trivially_copy_assignable<date::weekday_indexed>{}, "");
static_assert( std::is_trivially_move_constructible<date::weekday_indexed>{}, "");
static_assert( std::is_trivially_move_assignable<date::weekday_indexed>{}, "");

static_assert(std::is_nothrow_constructible<date::weekday_indexed, date::weekday,
                                                                   unsigned>{}, "");

int
main()
{
    using namespace date;

    constexpr weekday_indexed wdi = sun[1];
    static_assert(wdi.weekday() == sun, "");
    static_assert(wdi.index() == 1, "");
    static_assert(wdi.ok(), "");
    static_assert(wdi == weekday_indexed{sun, 1}, "");
    static_assert(wdi != weekday_indexed{sun, 2}, "");
    static_assert(wdi != weekday_indexed{mon, 1}, "");
    std::ostringstream os;
    os << wdi;
    assert(os.str() == "Sun[1]");
}
