// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>

// Test case was inspired by Bruno Dutra. Thanks!

enum class color {
    red,
    green,
    blue
};

std::ostream& operator <<(std::ostream& os, color c) {
    switch(c) {
        case color::red:
            os << "red";
            break;
        case color::green:
            os << "green";
            break;
        case color::blue:
            os << "blue";
            break;
    };

    return os;
}


struct my_constexpr {
    constexpr my_constexpr() {}
};

std::ostream& operator <<(std::ostream& os, my_constexpr) {
    return os << "{}";
}

struct reg {
    const int a;
    char b;
    const my_constexpr d;
    const color f;
    const char* g;
};

struct simple {
    int a;
    char b;
    short d;
};

struct empty{};


int main () {
    std::size_t control = 0;

    int v = {};
    boost::pfr::for_each_field(v, [&control](auto&& val, std::size_t i) {
        BOOST_TEST_EQ(i, control);
        (void)val;
        ++ control;
    });
    BOOST_TEST_EQ(control, 1);

    control = 0;
    int array[10] = {};
    boost::pfr::for_each_field(array, [&control](auto&& val, std::size_t i) {
        BOOST_TEST_EQ(i, control);
        (void)val;
        ++ control;
    });
    BOOST_TEST_EQ(control, 10);

    std::stringstream ss;
    boost::pfr::for_each_field(reg{42, 'a', {}, color::green, "hello world!"}, [&ss](auto&& val, std::size_t i) {
        if (i) {
            ss << ", ";
        }
        ss << val;
    });
    BOOST_TEST_EQ(std::string("42, a, {}, green, hello world!"), ss.str());
    ss.str("");

    control = 0;
    boost::pfr::for_each_field(reg{42, 'a', {}, color::green, "hello world!"}, [&ss, &control](auto&& val, auto i) {
        if (!!decltype(i)::value) {
            ss << ", ";
        }
        BOOST_TEST_EQ(decltype(i)::value, control);
        ++ control;
        ss << val;
    });
    BOOST_TEST_EQ(std::string("42, a, {}, green, hello world!"), ss.str());
    ss.str("");

    boost::pfr::for_each_field(reg{42, 'a', {}, color::green, "hello world!"}, [&ss](auto&& val) {
        ss << val << ' ';
    });
    BOOST_TEST_EQ(std::string("42 a {} green hello world! "), ss.str());
    ss.str("");

    std::cout << '\n';
    boost::pfr::for_each_field(simple{42, 'a', 3},  [&ss](auto&& val) {
        ss << val << ' ';
    });
    BOOST_TEST_EQ("42 a 3 ", ss.str());
    ss.str("");

    boost::pfr::for_each_field(empty{},  [&ss](auto&& val) {
        ss << val << ' ';
    });
    BOOST_TEST_EQ("", ss.str());
    ss.str("");

    return boost::report_errors();
}
