// The MIT License (MIT)
//
// Copyright (c) 2017 Howard Hinnant
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

// Test custom time zone support

#include "tz.h"

class OffsetZone
{
    std::chrono::minutes offset_;

public:
    explicit OffsetZone(std::chrono::minutes offset)
        : offset_{offset}
        {}

    template <class Duration>
        auto
        to_local(date::sys_time<Duration> tp) const
        {
            using namespace date;
            using namespace std::chrono;
            using LT = local_time<std::common_type_t<Duration, minutes>>;
            return LT{(tp + offset_).time_since_epoch()};
        }

    template <class Duration>
        auto
        to_sys(date::local_time<Duration> tp) const
        {
            using namespace date;
            using namespace std::chrono;
            using ST = sys_time<std::common_type_t<Duration, minutes>>;
            return ST{(tp - offset_).time_since_epoch()};
        }

    template <class Duration>
        date::sys_info
        get_info(date::sys_time<Duration> st) const
        {
            using namespace date;
            using namespace std::chrono;
            return {sys_seconds::min(), sys_seconds::max(), offset_,
                    minutes{0}, offset_ >= minutes{0} ? "+" + date::format("%H%M", offset_)
                                          : "-" + date::format("%H%M", -offset_)};
        }

    const OffsetZone* operator->() const {return this;}
};

#include <cassert>

int
main()
{
    using namespace date;
    using namespace std::chrono;
    auto now = system_clock::now();
    auto offset = hours{-4};
    zoned_time<system_clock::duration, OffsetZone> zt{OffsetZone{offset}, now};
    assert(zt.get_local_time().time_since_epoch() == now.time_since_epoch() + offset);
}
