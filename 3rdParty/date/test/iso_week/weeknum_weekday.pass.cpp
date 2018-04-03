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

// class weeknum_weekday
// {
//     iso_week::weeknum wn_;  // exposition only
//     iso_week::weekday wd_;  // exposition only
//
// public:
//     constexpr weeknum_weekday(const iso_week::weeknum& wn,
//                               const iso_week::weekday& wd) noexcept;
//
//     constexpr iso_week::weeknum weeknum() const noexcept;
//     constexpr iso_week::weekday weekday() const noexcept;
//
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const weeknum_weekday& x, const weeknum_weekday& y) noexcept;
// constexpr bool operator!=(const weeknum_weekday& x, const weeknum_weekday& y) noexcept;
// constexpr bool operator< (const weeknum_weekday& x, const weeknum_weekday& y) noexcept;
// constexpr bool operator> (const weeknum_weekday& x, const weeknum_weekday& y) noexcept;
// constexpr bool operator<=(const weeknum_weekday& x, const weeknum_weekday& y) noexcept;
// constexpr bool operator>=(const weeknum_weekday& x, const weeknum_weekday& y) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const weeknum_weekday& md);

#include "iso_week.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<iso_week::weeknum_weekday>{}, "");
static_assert(!std::is_default_constructible<iso_week::weeknum_weekday>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::weeknum_weekday>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::weeknum_weekday>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::weeknum_weekday>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::weeknum_weekday>{}, "");

static_assert(std::is_trivially_copyable<iso_week::weeknum_weekday>{}, "");
static_assert(std::is_standard_layout<iso_week::weeknum_weekday>{}, "");
static_assert(std::is_literal_type<iso_week::weeknum_weekday>{}, "");

static_assert( std::is_nothrow_constructible<iso_week::weeknum_weekday,
                                                 iso_week::weeknum,
                                                 iso_week::weekday>{}, "");

int
main()
{
    using namespace iso_week;

    constexpr auto wn_wd = weeknum_weekday{52_w, tue};
    static_assert(wn_wd.weeknum() == 52_w, "");
    static_assert(wn_wd.weekday() == tue, "");

    static_assert(wn_wd.ok(), "");

    static_assert(wn_wd == weeknum_weekday{52_w, tue}, "");
    static_assert(!(wn_wd != weeknum_weekday{52_w, tue}), "");
    static_assert(wn_wd < weeknum_weekday{52_w, wed}, "");

    std::ostringstream os;
    os << wn_wd;
    assert(os.str() == "W52-Tue");
}
