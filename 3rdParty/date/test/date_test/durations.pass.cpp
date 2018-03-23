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

// durations
//
// using days = std::chrono::duration
//     <int, std::ratio_multiply<std::ratio<24>, std::chrono::hours::period>>;
//
// using weeks = std::chrono::duration
//     <int, std::ratio_multiply<std::ratio<7>, days::period>>;
//
// using years = std::chrono::duration
//     <int, std::ratio_multiply<std::ratio<146097, 400>, days::period>>;
//
// using months = std::chrono::duration
//     <int, std::ratio_divide<years::period, std::ratio<12>>>;
//
// time_point
//
// using sys_days = std::chrono::time_point<std::chrono::system_clock, days>;

#include "date.h"

#include <type_traits>

static_assert(date::days{1} == std::chrono::hours{24}, "");

static_assert(date::weeks{1} == date::days{7}, "");

static_assert(date::months{12} == date::years{1}, "");
static_assert(date::days{30} < date::months{1} && date::months{1} < date::days{31}, "");
static_assert(date::weeks{4} < date::months{1} && date::months{1} < date::weeks{5}, "");

static_assert(date::years{400} == date::days{146097}, "");
static_assert(date::days{365} < date::years{1} && date::years{1} < date::days{366}, "");
static_assert(date::weeks{52} < date::years{1} && date::years{1} < date::weeks{53}, "");

static_assert(std::is_same<date::sys_days::duration, date::days>{}, "");

int
main()
{
}
