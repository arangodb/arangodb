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

// template <unsigned w>
// struct make_precision
// {
//     using type = std::chrono::duration<std::int64_t,
//                                        std::ratio<1, static_pow10<w>::value>>;
//     static constexpr unsigned width = w;
// };

#include "date.h"

#include <cstdint>
#include <type_traits>

int
main()
{
    using namespace date::detail;
    using namespace std;
    using namespace std::chrono;

    static_assert(make_precision<int64_t, 0>::width ==  0, "");
    static_assert(is_same<make_precision<int64_t, 0>::type, duration<int64_t, ratio<1, 1>>>{}, "");

    static_assert(make_precision<int64_t, 1>::width ==  1, "");
    static_assert(is_same<make_precision<int64_t, 1>::type, duration<int64_t, ratio<1, 10>>>{}, "");

    static_assert(make_precision<int64_t, 2>::width ==  2, "");
    static_assert(is_same<make_precision<int64_t, 2>::type, duration<int64_t, ratio<1, 100>>>{}, "");

    static_assert(make_precision<int64_t, 3>::width ==  3, "");
    static_assert(is_same<make_precision<int64_t, 3>::type, duration<int64_t, ratio<1, 1000>>>{}, "");

    static_assert(make_precision<int64_t, 18>::width ==  18, "");
    static_assert(is_same<make_precision<int64_t, 18>::type, duration<int64_t, ratio<1, 1000000000000000000>>>{}, "");

    static_assert(make_precision<int64_t, 19>::width ==  6, "");
    static_assert(is_same<make_precision<int64_t, 19>::type, microseconds>{}, "");

    static_assert(make_precision<int64_t, 20>::width ==  6, "");
    static_assert(is_same<make_precision<int64_t, 20>::type, microseconds>{}, "");
}
