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

// constexpr weekday sun{0};
// constexpr weekday mon{1};
// constexpr weekday tue{2};
// constexpr weekday wed{3};
// constexpr weekday thu{4};
// constexpr weekday fri{5};
// constexpr weekday sat{6};

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::weekday>{}, "");
static_assert( std::is_default_constructible<date::weekday>{}, "");
static_assert( std::is_trivially_copy_constructible<date::weekday>{}, "");
static_assert( std::is_trivially_copy_assignable<date::weekday>{}, "");
static_assert( std::is_trivially_move_constructible<date::weekday>{}, "");
static_assert( std::is_trivially_move_assignable<date::weekday>{}, "");

static_assert( std::is_nothrow_constructible<date::weekday, unsigned>{}, "");
static_assert( std::is_nothrow_constructible<date::weekday, date::sys_days>{}, "");
static_assert( std::is_nothrow_constructible<unsigned, date::weekday>{}, "");
static_assert(!std::is_convertible<unsigned, date::weekday>{}, "");
static_assert( std::is_convertible<date::sys_days, date::weekday>{}, "");
static_assert(!std::is_convertible<date::weekday, unsigned>{}, "");
static_assert(static_cast<unsigned>(date::weekday{1u}) == 1, "");

static_assert( date::weekday{0u}.ok(), "");
static_assert( date::weekday{1u}.ok(), "");
static_assert( date::weekday{2u}.ok(), "");
static_assert( date::weekday{3u}.ok(), "");
static_assert( date::weekday{4u}.ok(), "");
static_assert( date::weekday{5u}.ok(), "");
static_assert( date::weekday{6u}.ok(), "");
static_assert(!date::weekday{7u}.ok(), "");

void
test_weekday_arithmetic()
{
    using namespace date;
    constexpr unsigned a[7][7] =
    {// -    Sun Mon Tue Wed Thu Fri Sat
     /*Sun*/ {0,  6,  5,  4,  3,  2,  1},
     /*Mon*/ {1,  0,  6,  5,  4,  3,  2},
     /*Tue*/ {2,  1,  0,  6,  5,  4,  3},
     /*Wed*/ {3,  2,  1,  0,  6,  5,  4},
     /*Thu*/ {4,  3,  2,  1,  0,  6,  5},
     /*Fri*/ {5,  4,  3,  2,  1,  0,  6},
     /*Sat*/ {6,  5,  4,  3,  2,  1,  0}
    };
    for (unsigned x = 0; x < 7; ++x)
    {
        for (unsigned y = 0; y < 7; ++y)
        {
            assert(weekday{x} - weekday{y} == days{a[x][y]});
            assert(weekday{x} - days{a[x][y]} == weekday{y});
            assert(weekday{x} == weekday{y} + days{a[x][y]});
            assert(weekday{x} == days{a[x][y]} + weekday{y});
        }
    }
    for (unsigned x = 0; x < 7; ++x)
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
    for (unsigned x = 0; x < 7; ++x)
    {
        for (int y = -21; y < 21; ++y)
        {
            weekday wx{x};
            days dy{y};
            wx -= dy;
            assert(wx == weekday{x} + days{-y});
        }
    }
    for (unsigned x = 0; x < 7; ++x)
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

int
main()
{
    using namespace date;

    static_assert(sun == weekday{0u}, "");
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
