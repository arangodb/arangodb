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

// template <class Duration,
//           unsigned w = width<std::common_type<
//                                  Duration,
//                                  std::chrono::seconds>::type::period::den>::value>
// class decimal_format_seconds
// {
// public:
//     using precision = typename make_precision<w>::type;
//     static auto constexpr width = make_precision<w>::width;
// 
// private:
//     std::chrono::seconds s_;
//     precision            sub_s_;
// 
// public:
//     constexpr explicit decimal_format_seconds(const Duration& d) noexcept;
// 
//     constexpr std::chrono::seconds& seconds() noexcept;
//     constexpr std::chrono::seconds seconds() const noexcept;
//     constexpr precision subseconds() const noexcept;
//     constexpr precision to_duration() const noexcept;
// 
//     template <class CharT, class Traits>
//     friend
//     std::basic_ostream<CharT, Traits>&
//     operator<<(std::basic_ostream<CharT, Traits>& os, const decimal_format_seconds& x);
// };

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
    using namespace date::detail;
    using namespace std;
    using namespace std::chrono;

    {
        using D = decimal_format_seconds<minutes>;
        static_assert(is_same<D::precision, seconds>{}, "");
        static_assert(D::width == 0, "");
        D dfs{minutes{3}};
        assert(dfs.seconds() == seconds{180});
        assert(dfs.to_duration() == seconds{180});
        ostringstream out;
        out << dfs;
        assert(out.str() == "180");
    }
    {
        using D = decimal_format_seconds<seconds>;
        static_assert(is_same<D::precision, seconds>{}, "");
        static_assert(D::width == 0, "");
        D dfs{seconds{3}};
        assert(dfs.seconds() == seconds{3});
        assert(dfs.to_duration() == seconds{3});
        ostringstream out;
        out << dfs;
        assert(out.str() == "03");
    }
    {
        using D = decimal_format_seconds<milliseconds>;
        static_assert(D::width == 3, "");
        static_assert(is_same<D::precision, make_precision<D::rep, D::width>::type>{}, "");
        D dfs{seconds{3}};
        assert(dfs.seconds() == seconds{3});
        assert(dfs.to_duration() == seconds{3});
        assert(dfs.subseconds() == milliseconds{0});
        ostringstream out;
        out << dfs;
        assert(out.str() == "03.000");
    }
    {
        using D = decimal_format_seconds<milliseconds>;
        static_assert(D::width == 3, "");
        static_assert(is_same<D::precision, make_precision<D::rep, D::width>::type>{}, "");
        D dfs{milliseconds{3}};
        assert(dfs.seconds() == seconds{0});
        assert(dfs.to_duration() == milliseconds{3});
        assert(dfs.subseconds() == milliseconds{3});
        ostringstream out;
        out << dfs;
        assert(out.str() == "00.003");
    }
    {
        using D = decimal_format_seconds<microfortnights>;
        static_assert(D::width == 4, "");
        using S = make_precision<D::rep, D::width>::type;
        static_assert(is_same<D::precision, S>{}, "");
        D dfs{microfortnights{3}};
        assert(dfs.seconds() == seconds{3});
        assert(dfs.to_duration() == S{36288});
        assert(dfs.subseconds() == S{6288});
        ostringstream out;
        out << dfs;
        assert(out.str() == "03.6288");
    }
    {
        using CT = common_type<seconds, microfortnights>::type;
        using D = decimal_format_seconds<CT>;
        static_assert(D::width == 4, "");
        using S = make_precision<D::rep, D::width>::type;
        static_assert(is_same<D::precision, S>{}, "");
        D dfs{microfortnights{3}};
        assert(dfs.seconds() == seconds{3});
        assert(dfs.to_duration() == S{36288});
        assert(dfs.subseconds() == S{6288});
        ostringstream out;
        out << dfs;
        assert(out.str() == "03.6288");
    }
}
