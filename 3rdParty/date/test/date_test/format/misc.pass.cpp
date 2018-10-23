// The MIT License (MIT)
//
// Copyright (c) 2016 Howard Hinnant
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
#include <cassert>
#include <sstream>

template <class T>
void
test(const std::string& in_fmt, const std::string& input,
     const std::string& out_fmt, const std::string& output)
{
    using namespace date;
    std::istringstream in{input};
    T t;
    in >> parse(in_fmt, t);
    assert(!in.fail());
    auto s = format(out_fmt, t);
    assert(s == output);
}

int
main()
{
    using namespace date;
    test<year>("%Y", "2017", "%Y", "2017");
    test<month>("%m", "3", "%m", "03");
    test<day>("%d", "25", "%d", "25");
    test<year_month>("%Y-%m", "2017-03", "%Y-%m", "2017-03");
    test<year_month>("%y%m", "1703", "%Y-%m", "2017-03");
    test<month_day>("%m/%d", "3/25", "%m/%d", "03/25");
    test<weekday>("%w", "3", "%w", "3");
}
