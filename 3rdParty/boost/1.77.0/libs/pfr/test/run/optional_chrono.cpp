// Copyright (c) 2020-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>

#include <boost/pfr/core.hpp>

#include <chrono>

#include <optional>

// This class mimics libc++ implementation of std::chrono::duration with unfxed LWG3050
template <class Rep, class Period>
class bogus_duration {
public:
    bogus_duration() = default;

    template <class T>
    explicit bogus_duration(const T& val,
            typename std::enable_if<
                    std::is_convertible<T, Rep>::value // <= libstdc++ fix for LWG3050 is 's/T/const T&/g'
            >::type* = nullptr)
        : rep_(val)
    {}

    template <class Rep2, class Period2>
    bogus_duration(const bogus_duration<Rep2, Period2>& val,
            typename std::enable_if<std::is_convertible<Period2, Rep>::value>::type* = nullptr)
        : rep_(val)
    {}

    Rep get_rep() const { return rep_; }

private:
    Rep rep_{0};
};

struct struct_with_bogus_duration {
    std::optional<bogus_duration<long, char>> d0;
    std::optional<bogus_duration<long, char>> d1;
};

struct struct_with_optional {
    std::optional<std::chrono::seconds> a;
    std::optional<std::chrono::milliseconds> b;
    std::optional<std::chrono::microseconds> c;
    std::optional<std::chrono::nanoseconds> d;
    std::optional<std::chrono::steady_clock::duration> e;
    std::optional<std::chrono::system_clock::duration> f;
};

int main() {
    struct_with_optional val{
        std::chrono::seconds{1},
        std::chrono::seconds{2},
        std::chrono::seconds{3},
        std::chrono::seconds{4},
        std::chrono::seconds{5},
        std::chrono::seconds{6},
    };

    using boost::pfr::get;
    BOOST_TEST(get<0>(val) == std::chrono::seconds{1});
    BOOST_TEST(get<1>(val) == std::chrono::seconds{2});
    BOOST_TEST(get<2>(val) == std::chrono::seconds{3});
    BOOST_TEST(get<3>(val) == std::chrono::seconds{4});
    BOOST_TEST(get<3>(val) > std::chrono::seconds{0});
    BOOST_TEST(get<3>(val) > std::chrono::seconds{0});

    struct_with_bogus_duration val2{bogus_duration<long, char>{1}, bogus_duration<long, char>{2}};
    BOOST_TEST(get<0>(val2)->get_rep() == 1);
    BOOST_TEST(get<1>(val2)->get_rep() == 2);

    return boost::report_errors();
}

