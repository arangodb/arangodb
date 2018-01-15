// Copyright (c) 2014 Kohei Takahashi.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org for most recent version including documentation.

#include <boost/detail/lightweight_test.hpp>

#include <boost/assign/list_of.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/iterator/zip_iterator.hpp>
#include <vector>
#include <string>


int main()
{
    typedef TUPLE<
        std::vector<int>::iterator,
        std::vector<std::string>::iterator
    > iterator_tuple;

    std::vector<int>         vi = boost::assign::list_of(42)(72);
    std::vector<std::string> vs = boost::assign::list_of("kokoro")("pyonpyon");

    {
        boost::zip_iterator<iterator_tuple> i1(MAKE_TUPLE(vi.begin(), vs.begin()));
        boost::zip_iterator<iterator_tuple> i2 = i1;

        BOOST_TEST( i1      ==   i2);
        BOOST_TEST( i1++    ==   i2);
        BOOST_TEST( i1      ==  (i2 + 1));
        BOOST_TEST((i1 - 1) ==   i2);
        BOOST_TEST( i1--    == ++i2);
        BOOST_TEST( i1      == --i2);
    }

    {
        boost::zip_iterator<iterator_tuple> i1(MAKE_TUPLE(vi.begin(), vs.begin()));
        boost::zip_iterator<iterator_tuple> i2 = i1 + 1;

        BOOST_TEST( i1      !=   i2);
        BOOST_TEST( i1++    !=   i2);
        BOOST_TEST( i1      !=  (i2 + 1));
        BOOST_TEST((i1 - 1) !=   i2);
        BOOST_TEST( i1--    != ++i2);
        BOOST_TEST( i1      != --i2);
    }

    {
        boost::zip_iterator<iterator_tuple> i(MAKE_TUPLE(vi.begin(), vs.begin()));

        BOOST_TEST(boost::fusion::at_c<0>(* i     ) == 42);
        BOOST_TEST(boost::fusion::at_c<1>(* i     ) == "kokoro");
        BOOST_TEST(boost::fusion::at_c<0>(*(i + 1)) == 72);
        BOOST_TEST(boost::fusion::at_c<1>(*(i + 1)) == "pyonpyon");
    }

    {
        // Trac #12895
        boost::zip_iterator<
            TUPLE<int*, std::string*>
        > i(MAKE_TUPLE(&vi[0], &vs[0]));

        BOOST_TEST(boost::fusion::at_c<0>(* i     ) == 42);
        BOOST_TEST(boost::fusion::at_c<1>(* i     ) == "kokoro");
        BOOST_TEST(boost::fusion::at_c<0>(*(i + 1)) == 72);
        BOOST_TEST(boost::fusion::at_c<1>(*(i + 1)) == "pyonpyon");
    }

    {
        boost::zip_iterator<iterator_tuple> i1(MAKE_TUPLE(vi.begin(), vs.begin()));
        boost::zip_iterator<iterator_tuple> i2(MAKE_TUPLE(vi.end(),   vs.end()));

        BOOST_TEST((i2 - i1) == 2);
        ++i1;
        BOOST_TEST((i2 - i1) == 1);
        --i2;
        BOOST_TEST((i2 - i1) == 0);
    }

    return boost::report_errors();
}
