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

int
main()
{
    using namespace date;
    using namespace std::chrono;
    std::ostringstream os;
    os << format("%y", sys_days{jun/1/20001});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/20000});
    assert(os.str() == "00");

    os.str("");
    os << format("%y", sys_days{jun/1/19999});
    assert(os.str() == "99");

    os.str("");
    os << format("%y", sys_days{jun/1/2001});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/2000});
    assert(os.str() == "00");

    os.str("");
    os << format("%y", sys_days{jun/1/1999});
    assert(os.str() == "99");

    os.str("");
    os << format("%y", sys_days{jun/1/101});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/100});
    assert(os.str() == "00");

    os.str("");
    os << format("%y", sys_days{jun/1/99});
    assert(os.str() == "99");

    os.str("");
    os << format("%y", sys_days{jun/1/1});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/0});
    assert(os.str() == "00");

    os.str("");
    os << format("%y", sys_days{jun/1/-1});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/-99});
    assert(os.str() == "99");

    os.str("");
    os << format("%y", sys_days{jun/1/-100});
    assert(os.str() == "00");

    os.str("");
    os << format("%y", sys_days{jun/1/-101});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/-1999});
    assert(os.str() == "99");

    os.str("");
    os << format("%y", sys_days{jun/1/-2000});
    assert(os.str() == "00");

    os.str("");
    os << format("%y", sys_days{jun/1/-2001});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/-19999});
    assert(os.str() == "99");

    os.str("");
    os << format("%y", sys_days{jun/1/-20000});
    assert(os.str() == "00");

    os.str("");
    os << format("%y", sys_days{jun/1/-20001});
    assert(os.str() == "01");

    os.str("");
    os << format("%y", sys_days{jun/1/year::min()});
    assert(os.str() == "67");
}
