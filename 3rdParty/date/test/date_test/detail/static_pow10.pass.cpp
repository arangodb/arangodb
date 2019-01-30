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

// template <unsigned exp>
// struct static_pow10
// {
//     static constepxr std::uint64_t value = ...;
// };

#include "date.h"

#include <cassert>
#include <sstream>
#include <type_traits>

int
main()
{
    using namespace date::detail;
    static_assert(static_pow10<0>::value ==  1, "");
    static_assert(static_pow10<1>::value ==  10, "");
    static_assert(static_pow10<2>::value ==  100, "");
    static_assert(static_pow10<3>::value ==  1000, "");
    static_assert(static_pow10<4>::value ==  10000, "");
    static_assert(static_pow10<5>::value ==  100000, "");
    static_assert(static_pow10<6>::value ==  1000000, "");
    static_assert(static_pow10<7>::value ==  10000000, "");
    static_assert(static_pow10<8>::value ==  100000000, "");
    static_assert(static_pow10<9>::value ==  1000000000, "");
    static_assert(static_pow10<10>::value == 10000000000, "");
}
