// The MIT License (MIT)
//
// Copyright (c) 2017 Tomasz Kamiński
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

#include "tz.h"
#include <cassert>

int
main()
{
    using namespace date;
    using sys_clock = std::chrono::system_clock;

    // self
    {
       sys_days st(1997_y/dec/12);
       auto ut = utc_clock::from_sys(st);
       auto tt = tai_clock::from_utc(ut);

       assert(clock_cast<sys_clock>(st) == st);
       assert(clock_cast<utc_clock>(ut) == ut);
       assert(clock_cast<tai_clock>(tt) == tt);
    }

    // sys <-> utc
    {
       sys_days st(1997_y/dec/12);
       auto ut = utc_clock::from_sys(st);

       assert(clock_cast<utc_clock>(st) == ut);
       assert(clock_cast<sys_clock>(ut) == st);
    }

    // tai <-> utc
    {
       sys_days st(1997_y/dec/12);
       auto ut = utc_clock::from_sys(st);
       auto tt = tai_clock::from_utc(ut);

       assert(clock_cast<tai_clock>(ut) == tt);
       assert(clock_cast<utc_clock>(tt) == ut);
    }

    // tai <-> sys
    {
       sys_days st(1997_y/dec/12);
       auto ut = utc_clock::from_sys(st);
       auto tt = tai_clock::from_utc(ut);

       assert(clock_cast<tai_clock>(st) == tt);
       assert(clock_cast<sys_clock>(tt) == st);
    }

    // gps <-> utc
    {
       sys_days st(1997_y/dec/12);
       auto ut = utc_clock::from_sys(st);
       auto gt = gps_clock::from_utc(ut);

       assert(clock_cast<gps_clock>(ut) == gt);
       assert(clock_cast<utc_clock>(gt) == ut);
    }

    // gps <-> sys
    {
       sys_days st(1997_y/dec/12);
       auto ut = utc_clock::from_sys(st);
       auto gt = gps_clock::from_utc(ut);

       assert(clock_cast<gps_clock>(st) == gt);
       assert(clock_cast<sys_clock>(gt) == st);
    }

    // tai <-> gps
    {
       sys_days st(1997_y/dec/12);
       auto ut = utc_clock::from_sys(st);
       auto tt = tai_clock::from_utc(ut);
       auto gt = gps_clock::from_utc(ut);

       assert(clock_cast<gps_clock>(tt) == gt);
       assert(clock_cast<tai_clock>(gt) == tt);
    }
}
