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

// class month
// {
//     unsigned char m_;
// public:
//     explicit constexpr month(unsigned m) noexcept;
//
//     month& operator++()    noexcept;
//     month  operator++(int) noexcept;
//     month& operator--()    noexcept;
//     month  operator--(int) noexcept;
//
//     month& operator+=(const months& m) noexcept;
//     month& operator-=(const months& m) noexcept;
//
//     constexpr explicit operator unsigned() const noexcept;
//     constexpr bool ok() const noexcept;
// };
//
// constexpr bool operator==(const month& x, const month& y) noexcept;
// constexpr bool operator!=(const month& x, const month& y) noexcept;
// constexpr bool operator< (const month& x, const month& y) noexcept;
// constexpr bool operator> (const month& x, const month& y) noexcept;
// constexpr bool operator<=(const month& x, const month& y) noexcept;
// constexpr bool operator>=(const month& x, const month& y) noexcept;
//
// constexpr month  operator+(const month&  x, const months& y) noexcept;
// constexpr month  operator+(const months& x,  const month& y) noexcept;
// constexpr month  operator-(const month&  x, const months& y) noexcept;
// constexpr months operator-(const month&  x,  const month& y) noexcept;
//
// std::ostream& operator<<(std::ostream& os, const month& m);

// constexpr month jan{1};
// constexpr month feb{2};
// constexpr month mar{3};
// constexpr month apr{4};
// constexpr month may{5};
// constexpr month jun{6};
// constexpr month jul{7};
// constexpr month aug{8};
// constexpr month sep{9};
// constexpr month oct{10};
// constexpr month nov{11};
// constexpr month dec{12};

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

static_assert( std::is_trivially_destructible<date::month>{}, "");
static_assert( std::is_default_constructible<date::month>{}, "");
static_assert( std::is_trivially_copy_constructible<date::month>{}, "");
static_assert( std::is_trivially_copy_assignable<date::month>{}, "");
static_assert( std::is_trivially_move_constructible<date::month>{}, "");
static_assert( std::is_trivially_move_assignable<date::month>{}, "");

static_assert( std::is_nothrow_constructible<date::month, unsigned>{}, "");
static_assert( std::is_nothrow_constructible<unsigned, date::month>{}, "");
static_assert(!std::is_convertible<unsigned, date::month>{}, "");
static_assert(!std::is_convertible<date::month, unsigned>{}, "");
static_assert(static_cast<unsigned>(date::month{1}) == 1, "");

static_assert(!date::month{0}.ok(), "");
static_assert( date::month{1}.ok(), "");
static_assert( date::month{2}.ok(), "");
static_assert( date::month{3}.ok(), "");
static_assert( date::month{4}.ok(), "");
static_assert( date::month{5}.ok(), "");
static_assert( date::month{6}.ok(), "");
static_assert( date::month{7}.ok(), "");
static_assert( date::month{8}.ok(), "");
static_assert( date::month{9}.ok(), "");
static_assert( date::month{10}.ok(), "");
static_assert( date::month{11}.ok(), "");
static_assert( date::month{12}.ok(), "");
static_assert(!date::month{13}.ok(), "");

int
main()
{
    using namespace date;

    static_assert(jan == month{1}, "");
    static_assert(feb == month{2}, "");
    static_assert(mar == month{3}, "");
    static_assert(apr == month{4}, "");
    static_assert(may == month{5}, "");
    static_assert(jun == month{6}, "");
    static_assert(jul == month{7}, "");
    static_assert(aug == month{8}, "");
    static_assert(sep == month{9}, "");
    static_assert(oct == month{10}, "");
    static_assert(nov == month{11}, "");
    static_assert(dec == month{12}, "");

    static_assert(!(jan != jan), "");
    static_assert(  jan != feb, "");
    static_assert(  feb != jan, "");

    static_assert(!(jan <  jan), "");
    static_assert(  jan <  feb, "");
    static_assert(!(feb <  jan), "");

    static_assert(  jan <= jan, "");
    static_assert(  jan <= feb, "");
    static_assert(!(feb <= jan), "");

    static_assert(!(jan >  jan), "");
    static_assert(!(jan >  feb), "");
    static_assert(  feb >  jan, "");

    static_assert(  jan >= jan, "");
    static_assert(!(jan >= feb), "");
    static_assert(  feb >= jan, "");

    assert(mar + months{7} == oct);
    assert(mar + months{27} == jun);
    assert(months{7} + mar == oct);
    assert(months{27} + mar == jun);

    assert(mar - months{7} == aug);
    assert(mar - months{27} == dec);

    assert(mar - feb == months{1});
    assert(feb - mar == months{11});

#if __cplusplus >= 201402
    static_assert(mar + months{7} == oct, "");
    static_assert(mar + months{27} == jun, "");
    static_assert(months{7} + mar == oct, "");
    static_assert(months{27} + mar == jun, "");

    static_assert(mar - months{7} == aug, "");
    static_assert(mar - months{27} == dec, "");

    static_assert(mar - feb == months{1}, "");
    static_assert(feb - mar == months{11}, "");
#endif

    auto m = dec;
    assert(++m == jan);
    assert(m++ == jan);
    assert(m == feb);
    assert(m-- == feb);
    assert(m == jan);
    assert(--m == dec);
    assert((m += months{2}) == feb);
    assert((m -= months{2}) == dec);

    std::ostringstream os;
    os << m;
    assert(os.str() == "Dec");
    m += months{11};
    os.str("");
    os << m;
    assert(os.str() == "Nov");
}
