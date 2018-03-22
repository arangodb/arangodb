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

// enum {am = 1, pm};

// template <class Rep, class Period>
// class time_of_day<std::chrono::duration<Rep, Period>>
// {
// public:
//     using precision = std::chrono::std::chrono::duration<Rep, Period>;
//
//     constexpr explicit time_of_day(precision since_midnight) noexcept;
//     constexpr time_of_day(std::chrono::hours h, std::chrono::minutes m,
//                           std::chrono::seconds s, precision sub_s,
//                           unsigned md) noexcept;
//
//     constexpr std::chrono::hours hours() const noexcept;
//     constexpr std::chrono::minutes minutes() const noexcept;
//     constexpr std::chrono::seconds seconds() const noexcept;
//     constexpr precision subseconds() const noexcept;
//     constexpr unsigned mode() const noexcept;
//
//     constexpr explicit operator precision() const noexcept;
//     constexpr precision to_duration() const noexcept;
//
//     void make24() noexcept;
//     void make12() noexcept;
// };

// template <class Rep, class Period>
// std::ostream&
// operator<<(std::ostream& os, const time_of_day<std::chrono::duration<Rep, Period>>& t);

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

int
main()
{
    using namespace date;
    using namespace std;
    using namespace std::chrono;

    using tod = time_of_day<milliseconds>;

    static_assert(is_same<tod::precision::period, milliseconds::period>{}, "");

    static_assert( is_trivially_destructible<tod>{}, "");
    static_assert( is_default_constructible<tod>{}, "");
    static_assert( is_trivially_copy_constructible<tod>{}, "");
    static_assert( is_trivially_copy_assignable<tod>{}, "");
    static_assert( is_trivially_move_constructible<tod>{}, "");
    static_assert( is_trivially_move_assignable<tod>{}, "");

    static_assert(is_nothrow_constructible<tod, milliseconds>{}, "");
    static_assert(!is_convertible<milliseconds, tod>{}, "");

    static_assert(is_nothrow_constructible<tod::precision, tod>{}, "");
    static_assert(!is_convertible<tod, tod::precision>{}, "");

    constexpr tod t1 = tod{hours{13} + minutes{7} + seconds{5} + milliseconds{22}};
    static_assert(t1.hours() == hours{13}, "");
    static_assert(t1.minutes() == minutes{7}, "");
    static_assert(t1.seconds() == seconds{5}, "");
    static_assert(t1.subseconds() == milliseconds{22}, "");
    static_assert(t1.mode() == 0, "");
#if __cplusplus >= 201402
    static_assert(static_cast<tod::precision>(t1) == hours{13} + minutes{7}
                                                     + seconds{5} + milliseconds{22}, "");
    static_assert(t1.to_duration() == hours{13} + minutes{7} + seconds{5}
                                    + milliseconds{22}, "");
#endif

    auto t2 = t1;
    assert(t2.hours() == t1.hours());
    assert(t2.minutes() == t1.minutes());
    assert(t2.seconds() == t1.seconds());
    assert(t2.subseconds() == t1.subseconds());
    assert(t2.mode() == t1.mode());
    assert(t2.to_duration() == t1.to_duration());
    ostringstream os;
    os << t2;
    assert(os.str() == "13:07:05.022");
    t2.make12();
    os.str("");
    assert(t2.hours() == hours{1});
    assert(t2.minutes() == minutes{7});
    assert(t2.seconds() == seconds{5});
    assert(t2.subseconds() == milliseconds{22});
    assert(t2.mode() == pm);
    assert(t2.to_duration() == t1.to_duration());
    os << t2;
    assert(os.str() == "1:07:05.022pm");
    t2.make24();
    os.str("");
    assert(t2.hours() == hours{13});
    assert(t2.minutes() == minutes{7});
    assert(t2.seconds() == seconds{5});
    assert(t2.subseconds() == milliseconds{22});
    assert(t2.mode() == 0);
    assert(t2.to_duration() == t1.to_duration());
    os << t2;
    assert(os.str() == "13:07:05.022");
}
