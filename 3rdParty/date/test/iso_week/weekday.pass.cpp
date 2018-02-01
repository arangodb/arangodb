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

// class weekday
// {
//     unsigned char wd_;
// public:
//     explicit constexpr weekday(unsigned wd) noexcept;
//     constexpr weekday(date::weekday wd) noexcept;
//     constexpr weekday(const sys_days& dp) noexcept;
//
//     weekday& operator++()    noexcept;
//     weekday  operator++(int) noexcept;
//     weekday& operator--()    noexcept;
//     weekday  operator--(int) noexcept;
//
//     weekday& operator+=(const days& d) noexcept;
//     weekday& operator-=(const days& d) noexcept;
//
//     constexpr explicit operator unsigned() const noexcept;
//     constexpr operator date::weekday() const noexcept;
//     constexpr bool ok() const noexcept;
//
//     // tested with weekday_indexed
//     constexpr weekday_indexed operator[](unsigned index) const noexcept;
//     // tested with weekday_last
//     constexpr weekday_last    operator[](last_spec)      const noexcept;
// };

// constexpr bool operator==(const weekday& x, const weekday& y) noexcept;
// constexpr bool operator!=(const weekday& x, const weekday& y) noexcept;

// constexpr weekday operator+(const weekday& x, const days&    y) noexcept;
// constexpr weekday operator+(const days&    x, const weekday& y) noexcept;
// constexpr weekday operator-(const weekday& x, const days&    y) noexcept;
// constexpr days    operator-(const weekday& x, const weekday& y) noexcept;

// std::ostream& operator<<(std::ostream& os, const weekday& wd);

// constexpr weekday sun{7};
// constexpr weekday mon{1};
// constexpr weekday tue{2};
// constexpr weekday wed{3};
// constexpr weekday thu{4};
// constexpr weekday fri{5};
// constexpr weekday sat{6};

#include "iso_week.h"
#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

#include <iostream>

static_assert( std::is_trivially_destructible<iso_week::weekday>{}, "");
static_assert(!std::is_default_constructible<iso_week::weekday>{}, "");
static_assert( std::is_trivially_copy_constructible<iso_week::weekday>{}, "");
static_assert( std::is_trivially_copy_assignable<iso_week::weekday>{}, "");
static_assert( std::is_trivially_move_constructible<iso_week::weekday>{}, "");
static_assert( std::is_trivially_move_assignable<iso_week::weekday>{}, "");

static_assert(std::is_trivially_copyable<iso_week::weekday>{}, "");
static_assert(std::is_standard_layout<iso_week::weekday>{}, "");
static_assert(std::is_literal_type<iso_week::weekday>{}, "");

static_assert( std::is_nothrow_constructible<iso_week::weekday, unsigned>{}, "");
static_assert( std::is_nothrow_constructible<iso_week::weekday, iso_week::sys_days>{}, "");
static_assert( std::is_nothrow_constructible<iso_week::weekday, date::weekday>{}, "");
static_assert( std::is_nothrow_constructible<date::weekday, iso_week::weekday>{}, "");
static_assert( std::is_nothrow_constructible<unsigned, iso_week::weekday>{}, "");
static_assert(!std::is_convertible<unsigned, iso_week::weekday>{}, "");
static_assert( std::is_convertible<iso_week::sys_days, iso_week::weekday>{}, "");
static_assert( std::is_convertible<iso_week::weekday, date::weekday>{}, "");
static_assert( std::is_convertible<date::weekday, iso_week::weekday>{}, "");
static_assert(!std::is_convertible<iso_week::weekday, unsigned>{}, "");
static_assert(static_cast<unsigned>(iso_week::weekday{1u}) == 1, "");

static_assert(!iso_week::weekday{0u}.ok(), "");
static_assert( iso_week::weekday{1u}.ok(), "");
static_assert( iso_week::weekday{2u}.ok(), "");
static_assert( iso_week::weekday{3u}.ok(), "");
static_assert( iso_week::weekday{4u}.ok(), "");
static_assert( iso_week::weekday{5u}.ok(), "");
static_assert( iso_week::weekday{6u}.ok(), "");
static_assert( iso_week::weekday{7u}.ok(), "");

void
test_weekday_arithmetic()
{
    using namespace iso_week;
    constexpr unsigned a[7][7] =
    {// -    Mon Tue Wed Thu Fri Sat Sun
     /*Mon*/ {0,  6,  5,  4,  3,  2,  1},
     /*Tue*/ {1,  0,  6,  5,  4,  3,  2},
     /*Wed*/ {2,  1,  0,  6,  5,  4,  3},
     /*Thu*/ {3,  2,  1,  0,  6,  5,  4},
     /*Fri*/ {4,  3,  2,  1,  0,  6,  5},
     /*Sat*/ {5,  4,  3,  2,  1,  0,  6},
     /*Sun*/ {6,  5,  4,  3,  2,  1,  0}
    };
    for (unsigned x = 1; x <= 7; ++x)
    {
        for (unsigned y = 1; y <= 7; ++y)
        {
            assert(weekday{x} - weekday{y} == days{a[x-1][y-1]});
            assert(weekday{x} - days{a[x-1][y-1]} == weekday{y});
            assert(weekday{x} == weekday{y} + days{a[x-1][y-1]});
            assert(weekday{x} == days{a[x-1][y-1]} + weekday{y});
        }
    }
    for (unsigned x = 1; x <= 7; ++x)
    {
        for (int y = -21; y < 21; ++y)
        {
            weekday wx{x};
            days dy{y};
            wx += dy;
            weekday wz = weekday{x} + days{y};
            assert(wx.ok());
            assert(wz.ok());
            assert(wx == wz);
            assert(wx - weekday{x} == days{y % 7 + (y % 7 < 0 ? 7 : 0)});
        }
    }
    for (unsigned x = 1; x <= 7; ++x)
    {
        for (int y = -21; y < 21; ++y)
        {
            weekday wx{x};
            days dy{y};
            wx -= dy;
            assert(wx == weekday{x} + days{-y});
        }
    }
    for (unsigned x = 1; x <= 7; ++x)
    {
        weekday wx{x};
        assert(++wx - weekday{x} == days{1});
        assert(wx++ - weekday{x} == days{1});
        assert(wx - weekday{x} == days{2});
        assert(wx-- - weekday{x} == days{2});
        assert(wx - weekday{x} == days{1});
        assert(--wx - weekday{x} == days{0});
    }
}

void
test_with_date_weekday()
{
    auto constexpr d1 = iso_week::sun;
    static_assert(unsigned{d1} == 7, "");
    auto constexpr d2 = date::weekday{d1};
    static_assert(unsigned{d2} == 0, "");
    auto constexpr d3 = iso_week::weekday{d2};
    static_assert(unsigned{d3} == 7, "");
}

int
main()
{
    using namespace iso_week;

    static_assert(sun == weekday{7u}, "");
    static_assert(mon == weekday{1u}, "");
    static_assert(tue == weekday{2u}, "");
    static_assert(wed == weekday{3u}, "");
    static_assert(thu == weekday{4u}, "");
    static_assert(fri == weekday{5u}, "");
    static_assert(sat == weekday{6u}, "");

    static_assert(!(sun != sun), "");
    static_assert(  sun != mon, "");
    static_assert(  mon != sun, "");

    test_weekday_arithmetic();
    test_with_date_weekday();

    std::ostringstream os;

    os << sun;
    assert(os.str() == "Sun");
    os.str("");

    os << mon;
    assert(os.str() == "Mon");
    os.str("");

    os << tue;
    assert(os.str() == "Tue");
    os.str("");

    os << wed;
    assert(os.str() == "Wed");
    os.str("");

    os << thu;
    assert(os.str() == "Thu");
    os.str("");

    os << fri;
    assert(os.str() == "Fri");
    os.str("");

    os << sat;
    assert(os.str() == "Sat");
}
