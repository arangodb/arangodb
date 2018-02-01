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

struct bad_clock
{
  using duration = std::chrono::system_clock::duration;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<bad_clock, duration>;

  template<typename Duration>
  static
  int
  to_sys(std::chrono::time_point<bad_clock, Duration> const& tp)
  {
    return tp.time_since_epoch().count();
  }
};

int
main()
{
    using namespace date;
    using sys_clock = std::chrono::system_clock;

    auto bt = bad_clock::time_point();
    clock_cast<sys_clock>(bt);
}
