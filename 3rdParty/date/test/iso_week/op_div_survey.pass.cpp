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

#include "iso_week.h"

#include <type_traits>

template <class I, class J, class K>
constexpr
auto
test(I i, J j, K k) -> decltype(i/j/k)
{
    return i/j/k;
}

void
test(...)
{
}

int
main()
{
    using std::is_same;
    using namespace iso_week;

    static_assert(is_same<decltype(test(   1,    1,    1)), int>{}, "");
    static_assert(is_same<decltype(test(   1,    1,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(   1,    1,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(   1,    1,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   1,    1, last)), void>{}, "");

    static_assert(is_same<decltype(test(   1,  mon,    1)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  mon,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  mon,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  mon,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  mon, last)), void>{}, "");

    static_assert(is_same<decltype(test(   1,  1_w,    1)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_w,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_w,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_w,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_w, last)), void>{}, "");

    static_assert(is_same<decltype(test(   1,  1_y,    1)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_y,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_y,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_y,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   1,  1_y, last)), void>{}, "");

    static_assert(is_same<decltype(test(   1, last,    1)), void>{}, "");
    static_assert(is_same<decltype(test(   1, last,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(   1, last,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(   1, last,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   1, last, last)), void>{}, "");

    static_assert(is_same<decltype(test( mon,    1,    1)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( mon,    1,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( mon,    1,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( mon,    1,  1_y)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( mon,    1, last)), void>{}, "");

    static_assert(is_same<decltype(test( mon,  mon,    1)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  mon,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  mon,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  mon,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  mon, last)), void>{}, "");

    static_assert(is_same<decltype(test( mon,  1_w,    1)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( mon,  1_w,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  1_w,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  1_w,  1_y)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( mon,  1_w, last)), void>{}, "");

    static_assert(is_same<decltype(test( mon,  1_y,    1)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  1_y,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  1_y,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  1_y,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( mon,  1_y, last)), void>{}, "");

    static_assert(is_same<decltype(test( mon, last,    1)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test( mon, last,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( mon, last,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( mon, last,  1_y)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test( mon, last, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_w,    1,    1)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_w,    1,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,    1,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,    1,  1_y)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_w,    1, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_w,  mon,    1)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_w,  mon,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  mon,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  mon,  1_y)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_w,  mon, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_w,  1_w,    1)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_w,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_w,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_w,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_w, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_w,  1_y,    1)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_y,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_y,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_y,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w,  1_y, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_w, last,    1)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w, last,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w, last,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w, last,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_w, last, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_y,    1,    1)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_y,    1,  mon)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_y,    1,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,    1,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,    1, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_y,  mon,    1)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  mon,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  mon,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  mon,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  mon, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_y,  1_w,    1)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_w,  mon)), year_weeknum_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_w,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_w,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_w, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_y,  1_y,    1)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_y,  mon)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_y,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_y,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y,  1_y, last)), void>{}, "");

    static_assert(is_same<decltype(test( 1_y, last,    1)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_y, last,  mon)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test( 1_y, last,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y, last,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test( 1_y, last, last)), void>{}, "");

    static_assert(is_same<decltype(test(last,    1,    1)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test(last,    1,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(last,    1,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(last,    1,  1_y)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test(last,    1, last)), void>{}, "");

    static_assert(is_same<decltype(test(last,  mon,    1)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test(last,  mon,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(last,  mon,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(last,  mon,  1_y)), year_lastweek_weekday>{}, "");
    static_assert(is_same<decltype(test(last,  mon, last)), void>{}, "");

    static_assert(is_same<decltype(test(last,  1_w,    1)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_w,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_w,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_w,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_w, last)), void>{}, "");

    static_assert(is_same<decltype(test(last,  1_y,    1)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_y,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_y,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_y,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(last,  1_y, last)), void>{}, "");

    static_assert(is_same<decltype(test(last, last,    1)), void>{}, "");
    static_assert(is_same<decltype(test(last, last,  mon)), void>{}, "");
    static_assert(is_same<decltype(test(last, last,  1_w)), void>{}, "");
    static_assert(is_same<decltype(test(last, last,  1_y)), void>{}, "");
    static_assert(is_same<decltype(test(last, last, last)), void>{}, "");

}
