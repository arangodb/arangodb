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

// constexpr month_weekday_last operator/(const month& m, const weekday_last& wdl) noexcept;
// constexpr month_weekday_last operator/(int   m, const weekday_last& wdl) noexcept;
// constexpr month_weekday_last operator/(const weekday_last& wdl, const month& m) noexcept;
// constexpr month_weekday_last operator/(const weekday_last& wdl, int   m) noexcept;

#include "date.h"

int
main()
{
    using namespace date;

    static_assert(      aug/fri[last] == month_weekday_last{month{8}, weekday_last{weekday{5u}}}, "");
    static_assert(        8/fri[last] == month_weekday_last{month{8}, weekday_last{weekday{5u}}}, "");
    static_assert(fri[last]/aug       == month_weekday_last{month{8}, weekday_last{weekday{5u}}}, "");
    static_assert(fri[last]/8         == month_weekday_last{month{8}, weekday_last{weekday{5u}}}, "");
}
