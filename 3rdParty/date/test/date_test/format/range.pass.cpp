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
#include <type_traits>

using fortnights = std::chrono::duration<date::weeks::rep,
                                         std::ratio_multiply<std::ratio<2>,
                                                             date::weeks::period>>;

using microfortnights = std::chrono::duration<std::int64_t,
                                              std::ratio_multiply<fortnights::period,
                                                                  std::micro>>;

int
main()
{
    using namespace date;
    using namespace std::chrono;
    std::ostringstream os;
    os << format("%F %T", sys_days{jan/1/year::min()});
    assert(os.str() == "-32767-01-01 00:00:00");
    os.str("");
    os << format("%F %T", sys_days{dec/last/year::max()});
    assert(os.str() == "32767-12-31 00:00:00");
    os.str("");
    os << format("%F %T", sys_days{dec/last/year::max()} + hours{23} + minutes{59} +
                                                  seconds{59} + microseconds{999999});
    assert(os.str() == "32767-12-31 23:59:59.999999");
    os.str("");

    os << format("%Y-%m-%d %H:%M:%S", sys_days{jan/1/year::min()});
    assert(os.str() == "-32767-01-01 00:00:00");
    os.str("");
    os << format("%Y-%m-%d %H:%M:%S", sys_days{dec/last/year::max()});
    assert(os.str() == "32767-12-31 00:00:00");
    os.str("");
    os << format("%Y-%m-%d %H:%M:%S", sys_days{dec/last/year::max()} + hours{23} +
                                        minutes{59} + seconds{59} + microseconds{999999});
    assert(os.str() == "32767-12-31 23:59:59.999999");
    os.str("");

    os << format("%F %T", sys_days{jan/1/year::min()} + microfortnights{1});
    assert(os.str() == "-32767-01-01 00:00:01.2096");
    os.str("");
    os << format("%F %T", sys_days{dec/last/year::max()} + microfortnights{1});
    assert(os.str() == "32767-12-31 00:00:01.2096");
    os.str("");

    os << format("%F %T", jan/1/year::min());
    assert(os.str() == "-32767-01-01 00:00:00");
    os.str("");
    os << format("%F %T", dec/last/year::max());
    assert(os.str() == "32767-12-31 00:00:00");
    os.str("");
}
