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

// template <class Duration>
// class zoned_time
// {
// public:
//     using duration = typename std::common_type<Duration, std::chrono::seconds>::type;
// 
//              zoned_time();
//              zoned_time(const sys_time<Duration>& st);
//     explicit zoned_time(const time_zone* z);
//     explicit zoned_time(std::string_view name);
// 
//     template <class Duration2,
//               class = typename std::enable_if
//                       <
//                           std::is_convertible<sys_time<Duration2>,
//                                               sys_time<Duration>>::value
//                       >::type>
//         zoned_time(const zoned_time<Duration2>& zt) NOEXCEPT;
// 
//     zoned_time(const time_zone* z,    const local_time<Duration>& tp);
//     zoned_time(std::string_view name, const local_time<Duration>& tp);
//     zoned_time(const time_zone* z,    const local_time<Duration>& tp, choose c);
//     zoned_time(std::string_view name, const local_time<Duration>& tp, choose c);
// 
//     zoned_time(const time_zone* z,    const zoned_time<Duration>& zt);
//     zoned_time(std::string_view name, const zoned_time<Duration>& zt);
//     zoned_time(const time_zone* z,    const zoned_time<Duration>& zt, choose);
//     zoned_time(std::string_view name, const zoned_time<Duration>& zt, choose);
// 
//     zoned_time(const time_zone* z,    const sys_time<Duration>& st);
//     zoned_time(std::string_view name, const sys_time<Duration>& st);
// 
//     zoned_time& operator=(const sys_time<Duration>& st);
//     zoned_time& operator=(const local_time<Duration>& ut);
// 
//     explicit operator sys_time<duration>() const;
//     explicit operator local_time<duration>() const;
// 
//     const time_zone*     get_time_zone() const;
//     local_time<duration> get_local_time() const;
//     sys_time<duration>   get_sys_time() const;
//     sys_info             get_info() const;
// 
//     template <class Duration1, class Duration2>
//     friend
//     bool
//     operator==(const zoned_time<Duration1>& x, const zoned_time<Duration2>& y);
// 
//     template <class CharT, class Traits, class Duration1>
//     friend
//     std::basic_ostream<CharT, Traits>&
//     operator<<(std::basic_ostream<CharT, Traits>& os, const zoned_time<Duration1>& t);
// };
//
// using zoned_seconds = zoned_time<std::chrono::seconds>;
//
// template <class Duration1, class Duration2>
// inline
// bool
// operator!=(const zoned_time<Duration1>& x, const zoned_time<Duration2>& y);
//
// template <class Duration>
// zoned_time(sys_time<Duration>)
//     -> zoned_time<std::common_type_t<Duration, std::chrono::seconds>>;
//
// template <class Zone, class Duration>
// zoned_time(Zone, sys_time<Duration>)
//     -> zoned_time<std::common_type_t<Duration, std::chrono::seconds>>;
//
// template <class Zone, class Duration>
// zoned_time(Zone, local_time<Duration>, choose = choose::earliest)
//     -> zoned_time<std::common_type_t<Duration, std::chrono::seconds>>;
//
// template <class Zone, class Duration>
// zoned_time(Zone, zoned_time<Duration>, choose = choose::earliest)
//     -> zoned_time<std::common_type_t<Duration, std::chrono::seconds>>;

#include "tz.h"
#include <cassert>
#include <sstream>
#include <type_traits>

int
main()
{
    using namespace std;
    using namespace std::chrono;
    using namespace date;
    static_assert( is_nothrow_destructible<zoned_seconds>{}, "");
    static_assert( is_default_constructible<zoned_seconds>{}, "");
    static_assert( is_nothrow_copy_constructible<zoned_seconds>{}, "");
    static_assert( is_nothrow_copy_assignable<zoned_seconds>{}, "");
    static_assert( is_nothrow_move_constructible<zoned_seconds>{}, "");
    static_assert( is_nothrow_move_assignable<zoned_seconds>{}, "");

    static_assert(is_same<zoned_time<minutes>::duration, seconds>{}, "");
    static_assert(is_same<zoned_seconds::duration, seconds>{}, "");
    static_assert(is_same<zoned_time<milliseconds>::duration, milliseconds>{}, "");

    // zoned_time();
    {
        zoned_seconds zt;
        assert(zt.get_sys_time() == sys_seconds{});
        assert(zt.get_time_zone()->name() == "Etc/UTC");
    }

    // zoned_time(const sys_time<Duration>& st);
    {
        static_assert(!is_convertible<sys_days, zoned_seconds>{}, "");
        static_assert( is_constructible<zoned_seconds, sys_days>{}, "");
        static_assert( is_convertible<sys_seconds, zoned_seconds>{}, "");
        static_assert(!is_convertible<sys_time<milliseconds>, zoned_seconds>{}, "");
        static_assert(!is_constructible<zoned_seconds, sys_time<milliseconds>>{}, "");

        auto now = floor<seconds>(system_clock::now());
        zoned_seconds zt = now;
        assert(zt.get_sys_time() == now);
        assert(zt.get_time_zone()->name() == "Etc/UTC");
    }

    // explicit zoned_time(const time_zone* z);
    {
        static_assert(!is_convertible<const time_zone*, zoned_seconds>{}, "");
        static_assert( is_constructible<zoned_seconds, const time_zone*>{}, "");
        zoned_seconds zt{locate_zone("America/New_York")};
        assert(zt.get_sys_time() == sys_seconds{});
        assert(zt.get_time_zone()->name() == "America/New_York");
    }

    // explicit zoned_time(std::string_view name);
    {
        static_assert(!is_convertible<std::string, zoned_seconds>{}, "");
        static_assert( is_constructible<zoned_seconds, std::string>{}, "");
        static_assert( is_constructible<zoned_seconds, const char*>{}, "");
        static_assert( is_constructible<zoned_seconds, const char[3]>{}, "");
        zoned_seconds zt{"America/New_York"};
        assert(zt.get_sys_time() == sys_seconds{});
        assert(zt.get_time_zone()->name() == "America/New_York");
    }

    // template <class Duration2,
    //           class = typename std::enable_if
    //                   <
    //                       std::is_convertible<sys_time<Duration2>,
    //                                           sys_time<Duration>>::value
    //                   >::type>
    //     zoned_time(const zoned_time<Duration2>& zt) NOEXCEPT;
    {
        static_assert( is_convertible<zoned_time<days>, zoned_seconds>{}, "");
        static_assert(!is_constructible<zoned_time<days>, zoned_seconds>{}, "");
        zoned_time<days> zt1{"America/New_York", sys_days{2017_y/jul/5}};
        zoned_seconds zt2 = zt1;
        assert(zt2.get_sys_time() == sys_days{2017_y/jul/5});
        assert(zt2.get_time_zone()->name() == "America/New_York");
    }

    // zoned_time(const time_zone* z, const local_time<Duration>& tp);
    {
        static_assert( is_constructible<zoned_seconds, const time_zone*, local_days>{}, "");
        zoned_seconds zt = {locate_zone("America/New_York"), local_days{2017_y/jul/5}};
        assert(zt.get_local_time() == local_days{2017_y/jul/5});
        assert(zt.get_time_zone()->name() == "America/New_York");
        try
        {
            zoned_seconds zt1 = {locate_zone("America/New_York"),
                                 local_days{2017_y/mar/sun[2]} + hours{2} + minutes{15}};
            assert(false);
        }
        catch(const nonexistent_local_time&)
        {
        }
        try
        {
            zoned_seconds zt1 = {locate_zone("America/New_York"),
                                 local_days{2017_y/nov/sun[1]} + hours{1} + minutes{15}};
            assert(false);
        }
        catch(const ambiguous_local_time&)
        {
        }
    }

    // zoned_time(std::string_view name, const local_time<Duration>& tp);
    {
        static_assert( is_constructible<zoned_seconds, std::string, local_days>{}, "");
        static_assert( is_constructible<zoned_seconds, const char*, local_days>{}, "");
        zoned_seconds zt = {"America/New_York", local_days{2017_y/jul/5}};
        assert(zt.get_local_time() == local_days{2017_y/jul/5});
        assert(zt.get_time_zone()->name() == "America/New_York");
        try
        {
            zoned_seconds zt1 = {"America/New_York",
                                 local_days{2017_y/mar/sun[2]} + hours{2} + minutes{15}};
            assert(false);
        }
        catch(const nonexistent_local_time&)
        {
        }
        try
        {
            zoned_seconds zt1 = {"America/New_York",
                                 local_days{2017_y/nov/sun[1]} + hours{1} + minutes{15}};
            assert(false);
        }
        catch(const ambiguous_local_time&)
        {
        }
    }

    // zoned_time(const time_zone* z, const local_time<Duration>& tp, choose c);
    {
        static_assert( is_constructible<zoned_seconds, const time_zone*, local_days, choose>{}, "");
        zoned_seconds zt = {locate_zone("America/New_York"),
                            local_days{2017_y/jul/5} + hours{2} + minutes{15},
                            choose::earliest};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/5} + hours{6} + minutes{15});
        assert(zt.get_time_zone()->name() == "America/New_York");

        zt = {locate_zone("America/New_York"),
                            local_days{2017_y/jul/5} + hours{2} + minutes{15},
                            choose::latest};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/5} + hours{6} + minutes{15});
        assert(zt.get_time_zone()->name() == "America/New_York");

        static_assert( is_constructible<zoned_seconds, const time_zone*, local_days, choose>{}, "");
        zoned_seconds zt1 = {locate_zone("America/New_York"),
                             local_days{2017_y/mar/sun[2]} + hours{2} + minutes{15},
                             choose::earliest};
        assert(zt1.get_sys_time() == sys_days{2017_y/mar/12} + hours{7});
        assert(zt1.get_time_zone()->name() == "America/New_York");

        zoned_seconds zt2 = {locate_zone("America/New_York"),
                             local_days{2017_y/mar/sun[2]} + hours{2} + minutes{15},
                             choose::latest};
        assert(zt2.get_sys_time() == sys_days{2017_y/mar/12} + hours{7});
        assert(zt2.get_time_zone()->name() == "America/New_York");

        zoned_seconds zt3 = {locate_zone("America/New_York"),
                             local_days{2017_y/nov/sun[1]} + hours{1} + minutes{15},
                             choose::earliest};
        assert(zt3.get_sys_time() == sys_days{2017_y/nov/5} + hours{5} + minutes{15});
        assert(zt3.get_time_zone()->name() == "America/New_York");

        zoned_seconds zt4 = {locate_zone("America/New_York"),
                             local_days{2017_y/nov/sun[1]} + hours{1} + minutes{15},
                             choose::latest};
        assert(zt4.get_sys_time() == sys_days{2017_y/nov/5} + hours{6} + minutes{15});
        assert(zt4.get_time_zone()->name() == "America/New_York");
    }

    // zoned_time(std::string_view name, const local_time<Duration>& tp, choose c);
    {
        static_assert( is_constructible<zoned_seconds, std::string, local_days, choose>{}, "");
        static_assert( is_constructible<zoned_seconds, const char*, local_days, choose>{}, "");
        zoned_seconds zt = {"America/New_York",
                            local_days{2017_y/jul/5} + hours{2} + minutes{15},
                            choose::earliest};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/5} + hours{6} + minutes{15});
        assert(zt.get_time_zone()->name() == "America/New_York");

        zt = {"America/New_York",
                            local_days{2017_y/jul/5} + hours{2} + minutes{15},
                            choose::latest};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/5} + hours{6} + minutes{15});
        assert(zt.get_time_zone()->name() == "America/New_York");

        static_assert( is_constructible<zoned_seconds, const time_zone*, local_days, choose>{}, "");
        zoned_seconds zt1 = {"America/New_York",
                             local_days{2017_y/mar/sun[2]} + hours{2} + minutes{15},
                             choose::earliest};
        assert(zt1.get_sys_time() == sys_days{2017_y/mar/12} + hours{7});
        assert(zt1.get_time_zone()->name() == "America/New_York");

        zoned_seconds zt2 = {"America/New_York",
                             local_days{2017_y/mar/sun[2]} + hours{2} + minutes{15},
                             choose::latest};
        assert(zt2.get_sys_time() == sys_days{2017_y/mar/12} + hours{7});
        assert(zt2.get_time_zone()->name() == "America/New_York");

        zoned_seconds zt3 = {"America/New_York",
                             local_days{2017_y/nov/sun[1]} + hours{1} + minutes{15},
                             choose::earliest};
        assert(zt3.get_sys_time() == sys_days{2017_y/nov/5} + hours{5} + minutes{15});
        assert(zt3.get_time_zone()->name() == "America/New_York");

        zoned_seconds zt4 = {"America/New_York",
                             local_days{2017_y/nov/sun[1]} + hours{1} + minutes{15},
                             choose::latest};
        assert(zt4.get_sys_time() == sys_days{2017_y/nov/5} + hours{6} + minutes{15});
        assert(zt4.get_time_zone()->name() == "America/New_York");
    }

    // zoned_time(const time_zone* z, const sys_time<Duration>& st);
    {
        static_assert( is_constructible<zoned_seconds, const time_zone*, sys_days>{}, "");

        zoned_seconds zt = {locate_zone("America/New_York"),
                            sys_days{2017_y/jul/5} + hours{2} + minutes{15}};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/5} + hours{2} + minutes{15});
        assert(zt.get_time_zone()->name() == "America/New_York");
    }

    // zoned_time(std::string_view name, const sys_time<Duration>& st);
    {
        static_assert( is_constructible<zoned_seconds, std::string, sys_days>{}, "");
        static_assert( is_constructible<zoned_seconds, const char*, sys_days>{}, "");

        zoned_seconds zt = {"America/New_York",
                            sys_days{2017_y/jul/5} + hours{2} + minutes{15}};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/5} + hours{2} + minutes{15});
        assert(zt.get_time_zone()->name() == "America/New_York");
    }

    // zoned_time& operator=(const sys_time<Duration>& st);
    {
        static_assert( is_assignable<zoned_seconds, const sys_days&>{}, "");

        zoned_seconds zt{"America/New_York"};
        zt = sys_days{2017_y/jul/5};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/5});
        assert(zt.get_time_zone()->name() == "America/New_York");
    }

    // zoned_time& operator=(const local_time<Duration>& st);
    {
        static_assert( is_assignable<zoned_seconds, const local_days&>{}, "");

        zoned_seconds zt{"America/New_York"};
        zt = local_days{2017_y/jul/5};
        assert(zt.get_local_time() == local_days{2017_y/jul/5});
        assert(zt.get_time_zone()->name() == "America/New_York");
        try
        {
            zt = {"America/New_York",
                                 local_days{2017_y/mar/sun[2]} + hours{2} + minutes{15}};
            assert(false);
        }
        catch(const nonexistent_local_time&)
        {
            assert(zt.get_local_time() == local_days{2017_y/jul/5});
            assert(zt.get_time_zone()->name() == "America/New_York");
        }
        try
        {
            zt = {"America/New_York",
                                 local_days{2017_y/nov/sun[1]} + hours{1} + minutes{15}};
            assert(false);
        }
        catch(const ambiguous_local_time&)
        {
            assert(zt.get_local_time() == local_days{2017_y/jul/5});
            assert(zt.get_time_zone()->name() == "America/New_York");
        }
    }

    // explicit operator sys_time<duration>() const;
    {
        static_assert(!is_convertible<zoned_seconds, sys_seconds>{}, "");
        static_assert( is_constructible<sys_seconds, zoned_seconds>{}, "");
        auto now = floor<seconds>(system_clock::now());
        const zoned_seconds zt = {"America/New_York", now};
        assert(sys_seconds{zt} == now);
    }

    // explicit operator local_time<duration>() const;
    {
        static_assert(!is_convertible<zoned_seconds, local_seconds>{}, "");
        static_assert( is_constructible<local_seconds, zoned_seconds>{}, "");
        auto now = local_days{2017_y/jul/5} + hours{23} + minutes{1} + seconds{48};
        const zoned_seconds zt = {"America/New_York", now};
        assert(local_seconds{zt} == now);
    }

    // const time_zone* get_time_zone() const;
    {
        const zoned_seconds zt{"America/New_York"};
        assert(zt.get_time_zone() == locate_zone("America/New_York"));
    }

    // local_time<duration> get_local_time() const;
    {
        const zoned_seconds zt{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{9}};
        assert(zt.get_local_time() == local_days{2017_y/jul/5} +
                                      hours{23} + minutes{7} + seconds{9});
    }

    // sys_time<duration> get_sys_time() const;
    {
        const zoned_seconds zt{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{9}};
        assert(zt.get_sys_time() == sys_days{2017_y/jul/6} +
                                    hours{3} + minutes{7} + seconds{9});
    }

    // sys_info get_info() const;
    {
        const zoned_seconds zt{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{9}};
        auto info = zt.get_info();
        assert(info.begin == sys_days{2017_y/mar/12} + hours{7});
        assert(info.end == sys_days{2017_y/nov/5} + hours{6});
        assert(info.offset == hours{-4});
        assert(info.save != minutes{0});
        assert(info.abbrev == "EDT");
    }

    // template <class Duration1, class Duration2>
    // bool
    // operator==(const zoned_time<Duration1>& x, const zoned_time<Duration2>& y);
    {
        const zoned_seconds zt{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{9}};
        const zoned_seconds zt1{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{10}};
        assert(zt == zt);
        assert(!(zt == zt1));
    }

    // template <class Duration1, class Duration2>
    // bool
    // operator!=(const zoned_time<Duration1>& x, const zoned_time<Duration2>& y);
    {
        const zoned_seconds zt{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{9}};
        const zoned_seconds zt1{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{10}};
        assert(!(zt != zt));
        assert(zt != zt1);
    }

    // template <class CharT, class Traits, class Duration1>
    // std::basic_ostream<CharT, Traits>&
    // operator<<(std::basic_ostream<CharT, Traits>& os, const zoned_time<Duration1>& t);
    {
        const zoned_seconds zt{"America/New_York", sys_days{2017_y/jul/6} +
                                                   hours{3} + minutes{7} + seconds{9}};
        std::ostringstream test;
        test << zt;
        assert(test.str() == "2017-07-05 23:07:09 EDT");
    }
}
