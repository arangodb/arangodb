//  Copyright (c) 2014 Tomoki Imai
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/spirit/include/support_line_pos_iterator.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/assign.hpp>
#include <iostream>
#include <string>
#include <vector>

struct validation {
    validation()
    : line(), column(), current(), is_defined(false) {
    }

    validation(size_t line, size_t column, std::string current)
    : line(line), column(column), current(current), is_defined(true) {
    }

    size_t line;
    size_t column;
    std::string current;
    bool is_defined;
};

typedef std::vector<validation> validations;

void test(std::string const& input, validations const& validations) {
    typedef boost::spirit::line_pos_iterator<std::string::const_iterator> pos_iterator_t;

    pos_iterator_t const input_begin(input.begin());
    pos_iterator_t const input_end(input.end());
    pos_iterator_t position(input_begin);
    validations::const_iterator expected = validations.begin();

    for (; position != input_end && expected != validations.end(); ++position, ++expected) {
        if (!expected->is_defined)
        	continue;

        boost::iterator_range<pos_iterator_t> const range = get_current_line(input_begin, position, input_end);
        std::string const current(range.begin(), range.end());

        BOOST_TEST_EQ(expected->line, get_line(position));
        BOOST_TEST_EQ(expected->column, get_column(input_begin, position));
        BOOST_TEST_EQ(expected->current, current);
    }

    BOOST_TEST(position == input_end);
    BOOST_TEST(expected == validations.end());
}

// LR and CR

void testLRandCR(std::string const& line_break) {
    std::string const input = line_break + line_break;
    validations const validations = boost::assign::list_of<validation>
        (1,1,"")()
        (2,1,"")();
    test(input, validations);
}

void testLRandCR_foo_bar_git(std::string const& line_break) {
    std::string const input = "foo" + line_break + "bar" + line_break + "git";
    validations const validations = boost::assign::list_of<validation>
        (1,1,"foo")(1,2,"foo")(1,3,"foo")(1,4,"foo")()
        (2,1,"bar")(2,2,"bar")(2,3,"bar")(2,4,"bar")()
        (3,1,"git")(3,2,"git")(3,3,"git");
    test(input, validations);
}

void testLRandCR_bar_git(std::string const& line_break) {
    std::string const input = line_break + "bar" + line_break + "git";
    validations const validations = boost::assign::list_of<validation>
        (1,1,"")()
        (2,1,"bar")(2,2,"bar")(2,3,"bar")(2,4,"bar")()
        (3,1,"git")(3,2,"git")(3,3,"git");
    test(input, validations);
}

void testLRandCR_foo_bar(std::string const& line_break) {
    std::string const input = "foo" + line_break + "bar" + line_break;
    validations const validations = boost::assign::list_of<validation>
        (1,1,"foo")(1,2,"foo")(1,3,"foo")(1,4,"foo")()
        (2,1,"bar")(2,2,"bar")(2,3,"bar")(2,4,"bar")();
    test(input, validations);
}

// LR or CR

void testLRorCR(std::string const& line_break) {
    std::string const input = line_break + line_break;
    validations const validations = boost::assign::list_of<validation>
        (1,1,"")
        (2,1,"");
    test(input, validations);
}

void testLRorCR_foo_bar_git(std::string const& line_break) {
    std::string const input = "foo" + line_break + "bar" + line_break + "git";
    validations const validations = boost::assign::list_of<validation>
        (1,1,"foo")(1,2,"foo")(1,3,"foo")(1,4,"foo")
        (2,1,"bar")(2,2,"bar")(2,3,"bar")(2,4,"bar")
        (3,1,"git")(3,2,"git")(3,3,"git");
    test(input, validations);
}

void testLRorCR_bar_git(std::string const& line_break) {
    std::string const input = line_break + "bar" + line_break + "git";
    validations const validations = boost::assign::list_of<validation>
        (1,1,"")
        (2,1,"bar")(2,2,"bar")(2,3,"bar")(2,4,"bar")
        (3,1,"git")(3,2,"git")(3,3,"git");
    test(input, validations);
}

void testLRorCR_foo_bar(std::string const& line_break) {
    std::string const input = "foo" + line_break + "bar" + line_break;
    validations const validations = boost::assign::list_of<validation>
        (1,1,"foo")(1,2,"foo")(1,3,"foo")(1,4,"foo")
        (2,1,"bar")(2,2,"bar")(2,3,"bar")(2,4,"bar");
    test(input, validations);
}

int main()
{
    testLRandCR("\r\n");
    testLRandCR_foo_bar_git("\r\n");
    testLRandCR_bar_git("\r\n");
    testLRandCR_foo_bar("\r\n");

    testLRandCR("\n\r");
    testLRandCR_foo_bar_git("\n\r");
    testLRandCR_bar_git("\n\r");
    testLRandCR_foo_bar("\n\r");

    testLRorCR("\r");
    testLRorCR_foo_bar_git("\r");
    testLRorCR_bar_git("\r");
    testLRorCR_foo_bar("\r");

    testLRorCR("\n");
    testLRorCR_foo_bar_git("\n");
    testLRorCR_bar_git("\n");
    testLRorCR_foo_bar("\n");

    return boost::report_errors();
}
