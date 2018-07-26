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

// class weekday_last
// {
// public:
//     explicit constexpr weekday_last(const date::weekday& wd) noexcept;
//
//     constexpr date::weekday weekday() const noexcept;
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const weekday_last& x, const weekday_last& y) noexcept;
// constexpr bool operator!=(const weekday_last& x, const weekday_last& y) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const weekday_last& wdl);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::weekday_last>{}, "");
static_assert(!std::is_default_constructible<date::weekday_last>{}, "");
static_assert( std::is_trivially_copy_constructible<date::weekday_last>{}, "");
static_assert( std::is_trivially_copy_assignable<date::weekday_last>{}, "");
static_assert( std::is_trivially_move_constructible<date::weekday_last>{}, "");
static_assert( std::is_trivially_move_assignable<date::weekday_last>{}, "");

static_assert(std::is_nothrow_constructible<date::weekday_last, date::weekday>{}, "");
static_assert(!std::is_convertible<date::weekday, date::weekday_last>{}, "");

int
main()
{
    using namespace date;

    constexpr weekday_last wdl = sun[last];
    static_assert(wdl.weekday() == sun, "");
    static_assert(wdl.ok(), "");
    static_assert(wdl == weekday_last{sun}, "");
    static_assert(wdl != weekday_last{mon}, "");
    std::ostringstream os;
    os << wdl;
    assert(os.str() == "Sun[last]");
}
