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

#include "date.h"

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
    using namespace date;

    static_assert(is_same<decltype(test(        1,         1,         1)), int>{}, "");
    static_assert(is_same<decltype(test(        1,         1,       1_d)), month_day>{}, "");
    static_assert(is_same<decltype(test(        1,         1,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(        1,         1,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(        1,         1,      last)), month_day_last>{}, "");
    static_assert(is_same<decltype(test(        1,         1,    sun[1])), month_weekday>{}, "");
    static_assert(is_same<decltype(test(        1,         1, sun[last])), month_weekday_last>{}, "");

    static_assert(is_same<decltype(test(        1,       1_d,         1)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(        1,       1_d,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_d,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_d,       1_y)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(        1,       1_d,      last)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_d,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_d, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(        1,       jan,         1)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       jan,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       jan,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       jan,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       jan,      last)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       jan,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(        1,       jan, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(        1,       1_y,         1)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_y,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_y,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_y,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_y,      last)), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_y,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(        1,       1_y, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(        1,      last,         1)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(        1,      last,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(        1,      last,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(        1,      last,       1_y)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(        1,      last,      last)), void>{}, "");
    static_assert(is_same<decltype(test(        1,      last,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(        1,      last, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(        1,    sun[1],         1)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(        1,    sun[1],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(        1,    sun[1],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(        1,    sun[1],       1_y)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(        1,    sun[1],      last)), void>{}, "");
    static_assert(is_same<decltype(test(        1,    sun[1],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(        1,    sun[1], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(        1, sun[last],         1)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(        1, sun[last],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(        1, sun[last],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(        1, sun[last],       1_y)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(        1, sun[last],      last)), void>{}, "");
    static_assert(is_same<decltype(test(        1, sun[last],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(        1, sun[last], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_d,         1,         1)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_d,         1,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,         1,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,         1,       1_y)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_d,         1,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,         1,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,         1, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_d,       1_d,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_d,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_d,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_d,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_d,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_d,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_d, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_d,       jan,         1)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_d,       jan,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       jan,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       jan,       1_y)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_d,       jan,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       jan,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       jan, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_d,       1_y,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_y,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_y,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_y,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_y,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_y,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,       1_y, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_d,      last,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,      last,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,      last,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,      last,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,      last,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,      last,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,      last, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_d,    sun[1],         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,    sun[1],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,    sun[1],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,    sun[1],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,    sun[1],      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,    sun[1],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_d,    sun[1], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_d, sun[last],         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d, sun[last],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d, sun[last],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d, sun[last],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d, sun[last],      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_d, sun[last],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_d, sun[last], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      jan,         1,         1)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      jan,         1,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,         1,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,         1,       1_y)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      jan,         1,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,         1,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      jan,         1, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      jan,       1_d,         1)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_d,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_d,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_d,       1_y)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_d,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_d,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_d, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      jan,       jan,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       jan,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       jan,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       jan,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       jan,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       jan,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       jan, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      jan,       1_y,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_y,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_y,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_y,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_y,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_y,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      jan,       1_y, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      jan,      last,         1)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(      jan,      last,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,      last,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,      last,       1_y)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(      jan,      last,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,      last,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      jan,      last, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      jan,    sun[1],         1)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(      jan,    sun[1],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,    sun[1],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,    sun[1],       1_y)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(      jan,    sun[1],      last)), void>{}, "");
    static_assert(is_same<decltype(test(      jan,    sun[1],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      jan,    sun[1], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      jan, sun[last],         1)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(      jan, sun[last],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      jan, sun[last],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      jan, sun[last],       1_y)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(      jan, sun[last],      last)), void>{}, "");
    static_assert(is_same<decltype(test(      jan, sun[last],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      jan, sun[last], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_y,         1,         1)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_y,         1,       1_d)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_y,         1,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,         1,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,         1,      last)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(      1_y,         1,    sun[1])), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(      1_y,         1, sun[last])), year_month_weekday_last>{}, "");

    static_assert(is_same<decltype(test(      1_y,       1_d,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_d,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_d,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_d,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_d,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_d,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_d, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_y,       jan,         1)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_y,       jan,       1_d)), year_month_day>{}, "");
    static_assert(is_same<decltype(test(      1_y,       jan,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       jan,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       jan,      last)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(      1_y,       jan,    sun[1])), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(      1_y,       jan, sun[last])), year_month_weekday_last>{}, "");

    static_assert(is_same<decltype(test(      1_y,       1_y,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_y,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_y,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_y,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_y,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_y,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,       1_y, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_y,      last,         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,      last,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,      last,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,      last,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,      last,      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,      last,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,      last, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_y,    sun[1],         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,    sun[1],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,    sun[1],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,    sun[1],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,    sun[1],      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,    sun[1],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_y,    sun[1], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(      1_y, sun[last],         1)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y, sun[last],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y, sun[last],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y, sun[last],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y, sun[last],      last)), void>{}, "");
    static_assert(is_same<decltype(test(      1_y, sun[last],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(      1_y, sun[last], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(     last,         1,         1)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(     last,         1,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(     last,         1,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(     last,         1,       1_y)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(     last,         1,      last)), void>{}, "");
    static_assert(is_same<decltype(test(     last,         1,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(     last,         1, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(     last,       1_d,         1)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_d,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_d,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_d,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_d,      last)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_d,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_d, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(     last,       jan,         1)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(     last,       jan,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       jan,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       jan,       1_y)), year_month_day_last>{}, "");
    static_assert(is_same<decltype(test(     last,       jan,      last)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       jan,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(     last,       jan, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(     last,       1_y,         1)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_y,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_y,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_y,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_y,      last)), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_y,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(     last,       1_y, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(     last,      last,         1)), void>{}, "");
    static_assert(is_same<decltype(test(     last,      last,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(     last,      last,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(     last,      last,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(     last,      last,      last)), void>{}, "");
    static_assert(is_same<decltype(test(     last,      last,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(     last,      last, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(     last,    sun[1],         1)), void>{}, "");
    static_assert(is_same<decltype(test(     last,    sun[1],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(     last,    sun[1],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(     last,    sun[1],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(     last,    sun[1],      last)), void>{}, "");
    static_assert(is_same<decltype(test(     last,    sun[1],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(     last,    sun[1], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(     last, sun[last],         1)), void>{}, "");
    static_assert(is_same<decltype(test(     last, sun[last],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(     last, sun[last],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(     last, sun[last],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(     last, sun[last],      last)), void>{}, "");
    static_assert(is_same<decltype(test(     last, sun[last],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(     last, sun[last], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(   sun[1],         1,         1)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(   sun[1],         1,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],         1,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],         1,       1_y)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(   sun[1],         1,      last)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],         1,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],         1, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(   sun[1],       1_d,         1)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_d,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_d,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_d,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_d,      last)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_d,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_d, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(   sun[1],       jan,         1)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       jan,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       jan,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       jan,       1_y)), year_month_weekday>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       jan,      last)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       jan,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       jan, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(   sun[1],       1_y,         1)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_y,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_y,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_y,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_y,      last)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_y,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],       1_y, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(   sun[1],      last,         1)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],      last,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],      last,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],      last,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],      last,      last)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],      last,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],      last, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(   sun[1],    sun[1],         1)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],    sun[1],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],    sun[1],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],    sun[1],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],    sun[1],      last)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],    sun[1],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1],    sun[1], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(   sun[1], sun[last],         1)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1], sun[last],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1], sun[last],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1], sun[last],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1], sun[last],      last)), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1], sun[last],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(   sun[1], sun[last], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(sun[last],         1,         1)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(sun[last],         1,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],         1,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],         1,       1_y)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(sun[last],         1,      last)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],         1,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],         1, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(sun[last],       1_d,         1)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_d,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_d,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_d,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_d,      last)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_d,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_d, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(sun[last],       jan,         1)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(sun[last],       jan,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       jan,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       jan,       1_y)), year_month_weekday_last>{}, "");
    static_assert(is_same<decltype(test(sun[last],       jan,      last)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       jan,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       jan, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(sun[last],       1_y,         1)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_y,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_y,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_y,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_y,      last)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_y,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],       1_y, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(sun[last],      last,         1)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],      last,       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],      last,       jan)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],      last,       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],      last,      last)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],      last,    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],      last, sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(sun[last],    sun[1],         1)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],    sun[1],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],    sun[1],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],    sun[1],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],    sun[1],      last)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],    sun[1],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(sun[last],    sun[1], sun[last])), void>{}, "");

    static_assert(is_same<decltype(test(sun[last], sun[last],         1)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last], sun[last],       1_d)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last], sun[last],       jan)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last], sun[last],       1_y)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last], sun[last],      last)), void>{}, "");
    static_assert(is_same<decltype(test(sun[last], sun[last],    sun[1])), void>{}, "");
    static_assert(is_same<decltype(test(sun[last], sun[last], sun[last])), void>{}, "");
}
