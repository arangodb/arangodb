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

// constexpr year_month_day_last operator/(const year_month& ym, last_spec) noexcept;
// constexpr year_month_day_last operator/(const year& y, const month_day_last& mdl) noexcept;
// constexpr year_month_day_last operator/(int y, const month_day_last& mdl) noexcept;
// constexpr year_month_day_last operator/(const month_day_last& mdl, const year& y) noexcept;
// constexpr year_month_day_last operator/(const month_day_last& mdl, int y) noexcept;

#include "date.h"

int
main()
{
    using namespace date;

    static_assert(2015_y/aug/last       == year_month_day_last{year{2015}, month_day_last{month{8}}}, "");
    static_assert(    2015_y/(aug/last) == year_month_day_last{year{2015}, month_day_last{month{8}}}, "");
    static_assert(      2015/(aug/last) == year_month_day_last{year{2015}, month_day_last{month{8}}}, "");
    static_assert(  aug/last/2015_y     == year_month_day_last{year{2015}, month_day_last{month{8}}}, "");
    static_assert(  aug/last/2015       == year_month_day_last{year{2015}, month_day_last{month{8}}}, "");
}
