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

// This test is meant to maintain a record of the sizeof each type.

#include "date.h"

int
main()
{
    using namespace date;

    static_assert(sizeof(days)                    == 4, "");
    static_assert(sizeof(weeks)                   == 4, "");
    static_assert(sizeof(months)                  == 4, "");
    static_assert(sizeof(years)                   == 4, "");

    static_assert(sizeof(sys_days)               == 4, "");

    static_assert(sizeof(last_spec)               == 1, "");

    static_assert(sizeof(day)                     == 1, "");
    static_assert(sizeof(month)                   == 1, "");
    static_assert(sizeof(year)                    == 2, "");

    static_assert(sizeof(weekday)                 == 1, "");
    static_assert(sizeof(weekday_indexed)         == 1, "");
    static_assert(sizeof(weekday_last)            == 1, "");

    static_assert(sizeof(month_day)               == 2, "");
    static_assert(sizeof(month_day_last)          == 1, "");
    static_assert(sizeof(month_weekday)           == 2, "");
    static_assert(sizeof(month_weekday_last)      == 2, "");

    static_assert(sizeof(year_month)              == 4, "");

    static_assert(sizeof(year_month_day)          == 4, "");
    static_assert(sizeof(year_month_day_last)     == 4, "");
    static_assert(sizeof(year_month_weekday)      == 4, "");
    static_assert(sizeof(year_month_weekday_last) == 4, "");

    // sizeof time_of_day varies
}
