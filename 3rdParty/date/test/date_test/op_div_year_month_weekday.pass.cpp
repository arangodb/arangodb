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

// constexpr year_month_weekday operator/(const year_month& ym, const weekday_indexed& wdi) noexcept;
// constexpr year_month_weekday operator/(const year&        y, const month_weekday&   mwd) noexcept;
// constexpr year_month_weekday operator/(int                y, const month_weekday&   mwd) noexcept;
// constexpr year_month_weekday operator/(const month_weekday& mwd, const year&          y) noexcept;
// constexpr year_month_weekday operator/(const month_weekday& mwd, int                  y) noexcept;

#include "date.h"

int
main()
{
    using namespace date;

    static_assert(2015_y/aug/fri[2]       == year_month_weekday{year{2015}, month{8}, weekday_indexed{weekday{5u}, 2}}, "");
    static_assert(    2015_y/(aug/fri[2]) == year_month_weekday{year{2015}, month{8}, weekday_indexed{weekday{5u}, 2}}, "");
    static_assert(      2015/(aug/fri[2]) == year_month_weekday{year{2015}, month{8}, weekday_indexed{weekday{5u}, 2}}, "");
    static_assert(aug/fri[2]/2015_y       == year_month_weekday{year{2015}, month{8}, weekday_indexed{weekday{5u}, 2}}, "");
    static_assert(aug/fri[2]/2015         == year_month_weekday{year{2015}, month{8}, weekday_indexed{weekday{5u}, 2}}, "");
}
