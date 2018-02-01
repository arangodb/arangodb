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

// class year_weeknum
// {
//     iso_week::year    y_;
//     iso_week::weeknum wn_;
//
// public:
//     constexpr year_weeknum(const iso_week::year& y, const iso_week::weeknum& wn) noexcept;
//
//     constexpr iso_week::year    year()    const noexcept;
//     constexpr iso_week::weeknum weeknum() const noexcept;
//
//     year_weeknum& operator+=(const years& dy) noexcept;
//     year_weeknum& operator-=(const years& dy) noexcept;
//
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const year_weeknum& x, const year_weeknum& y) noexcept;
// constexpr bool operator!=(const year_weeknum& x, const year_weeknum& y) noexcept;
// constexpr bool operator< (const year_weeknum& x, const year_weeknum& y) noexcept;
// constexpr bool operator> (const year_weeknum& x, const year_weeknum& y) noexcept;
// constexpr bool operator<=(const year_weeknum& x, const year_weeknum& y) noexcept;
// constexpr bool operator>=(const year_weeknum& x, const year_weeknum& y) noexcept;
//
// constexpr year_weeknum operator+(const year_weeknum& ym, const years& dy) noexcept;
// constexpr year_weeknum operator+(const years& dy, const year_weeknum& ym) noexcept;
// constexpr year_weeknum operator-(const year_weeknum& ym, const years& dy) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const year_weeknum& ym);

#include "iso_week.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<iso_week::year_weeknum>{}, "");
static_assert(!std::is_default_constructible<iso_week::year_weeknum>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::year_weeknum>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::year_weeknum>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::year_weeknum>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::year_weeknum>{}, "");

static_assert(std::is_trivially_copyable<iso_week::year_weeknum>{}, "");
static_assert(std::is_standard_layout<iso_week::year_weeknum>{}, "");
static_assert(std::is_literal_type<iso_week::year_weeknum>{}, "");

static_assert( std::is_nothrow_constructible<iso_week::year_weeknum,
                                                 iso_week::year,
                                                 iso_week::weeknum>{}, "");

int
main()
{
    using namespace iso_week;

    constexpr auto y_wn = year_weeknum{2015_y, 52_w};
    static_assert(y_wn.year() == 2015_y, "");
    static_assert(y_wn.weeknum() == 52_w, "");

    static_assert(y_wn.ok(), "");

    static_assert(y_wn == year_weeknum{2015_y, 52_w}, "");
    static_assert(!(y_wn != year_weeknum{2015_y, 52_w}), "");
    static_assert(y_wn < year_weeknum{2015_y, 53_w}, "");

    auto y_wn2 = y_wn + years{2};
    assert((y_wn2 == year_weeknum{2017_y, 52_w}));
    y_wn2 -= years{1};
    assert((y_wn2 == year_weeknum{2016_y, 52_w}));

    std::ostringstream os;
    os << y_wn;
    assert(os.str() == "2015-W52");
}
