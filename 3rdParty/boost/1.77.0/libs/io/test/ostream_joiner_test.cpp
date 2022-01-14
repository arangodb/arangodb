/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/io/ostream_joiner.hpp>
#include <boost/core/is_same.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>

void test_char_type()
{
    BOOST_TEST((boost::core::is_same<char,
        boost::io::ostream_joiner<const char*>::char_type>::value));
}

void test_traits_type()
{
    BOOST_TEST((boost::core::is_same<std::char_traits<char>,
        boost::io::ostream_joiner<const char*>::traits_type>::value));
}

void test_ostream_type()
{
    BOOST_TEST((boost::core::is_same<std::ostream,
        boost::io::ostream_joiner<const char*>::ostream_type>::value));
}

void test_iterator_category()
{
    BOOST_TEST((boost::core::is_same<std::output_iterator_tag,
        boost::io::ostream_joiner<const char*>::iterator_category>::value));
}

void test_value_type()
{
    BOOST_TEST((boost::core::is_same<void,
        boost::io::ostream_joiner<const char*>::value_type>::value));
}

void test_difference_type()
{
    BOOST_TEST((boost::core::is_same<void,
        boost::io::ostream_joiner<const char*>::difference_type>::value));
}

void test_pointer()
{
    BOOST_TEST((boost::core::is_same<void,
        boost::io::ostream_joiner<const char*>::pointer>::value));
}

void test_reference()
{
    BOOST_TEST((boost::core::is_same<void,
        boost::io::ostream_joiner<const char*>::reference>::value));
}

void test_construct()
{
    std::ostringstream o;
    boost::io::ostream_joiner<const char*> j(o, ",");
    BOOST_TEST(o.str().empty());
}

void test_assign()
{
    std::ostringstream o;
    boost::io::ostream_joiner<const char*> j(o, ",");
    j = 1;
    BOOST_TEST_EQ(o.str(), "1");
    j = '2';
    BOOST_TEST_EQ(o.str(), "1,2");
    j = "3";
    BOOST_TEST_EQ(o.str(), "1,2,3");
}

void test_increment()
{
    std::ostringstream o;
    boost::io::ostream_joiner<const char*> j(o, ",");
    BOOST_TEST_EQ(&++j, &j);
}

void test_post_increment()
{
    std::ostringstream o;
    boost::io::ostream_joiner<const char*> j(o, ",");
    BOOST_TEST_EQ(&j++, &j);
}

void test_value()
{
    std::ostringstream o;
    boost::io::ostream_joiner<const char*> j(o, ",");
    BOOST_TEST_EQ(&*j, &j);
}

int main()
{
    test_char_type();
    test_traits_type();
    test_ostream_type();
    test_iterator_category();
    test_value_type();
    test_difference_type();
    test_pointer();
    test_reference();
    test_construct();
    test_assign();
    test_increment();
    test_post_increment();
    test_value();
    return boost::report_errors();
}
