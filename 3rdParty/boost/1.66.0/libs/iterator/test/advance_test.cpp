// Copyright (C) 2017 Michel Morin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <vector>
#include <list>
#include <boost/container/slist.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/iterator/advance.hpp>
#include <boost/iterator/transform_iterator.hpp>

int twice(int x) { return x + x; }

template <typename Iterator>
void test_advance(Iterator it_from, Iterator it_to, int n)
{
    boost::advance(it_from,  n);
    BOOST_TEST(it_from == it_to);
}

int main()
{
    int array[3] = {1, 2, 3};
    int* ptr1 = array;
    int* ptr2 = array + 3;

    {
        test_advance(ptr1, ptr2,  3);
        test_advance(ptr2, ptr1, -3);

        test_advance(
            boost::make_transform_iterator(ptr1, twice)
          , boost::make_transform_iterator(ptr2, twice)
          , 3
        );
        test_advance(
            boost::make_transform_iterator(ptr2, twice)
          , boost::make_transform_iterator(ptr1, twice)
          , -3
        );
    }

    {
        std::vector<int> ints(ptr1, ptr2);
        test_advance(ints.begin(), ints.end(),  3);
        test_advance(ints.end(), ints.begin(), -3);

        test_advance(
            boost::make_transform_iterator(ints.begin(), twice)
          , boost::make_transform_iterator(ints.end(), twice)
          , 3
        );
        test_advance(
            boost::make_transform_iterator(ints.end(), twice)
          , boost::make_transform_iterator(ints.begin(), twice)
          , -3
        );
    }

    {
        std::list<int> ints(ptr1, ptr2);
        test_advance(ints.begin(), ints.end(),  3);
        test_advance(ints.end(), ints.begin(), -3);

        test_advance(
            boost::make_transform_iterator(ints.begin(), twice)
          , boost::make_transform_iterator(ints.end(), twice)
          , 3
        );
        test_advance(
            boost::make_transform_iterator(ints.end(), twice)
          , boost::make_transform_iterator(ints.begin(), twice)
          , -3
        );
    }

    {
        boost::container::slist<int> ints(ptr1, ptr2);
        test_advance(ints.begin(), ints.end(),  3);

        test_advance(
            boost::make_transform_iterator(ints.begin(), twice)
          , boost::make_transform_iterator(ints.end(), twice)
          , 3
        );
    }

    return boost::report_errors();
}
