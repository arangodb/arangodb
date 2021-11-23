// Boost.Bimap
//
// Copyright (c) 2006-2007 Matias Capeletto
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  VC++ 8.0 warns on usage of certain Standard Library and API functions that
//  can be cause buffer overruns or other possible security issues if misused.
//  See https://web.archive.org/web/20071014014301/http://msdn.microsoft.com/msdnmag/issues/05/05/SafeCandC/default.aspx
//  But the wording of the warning is misleading and unsettling, there are no
//  portable alternative functions, and VC++ 8.0's own libraries use the
//  functions in question. So turn off the warnings.
#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_DEPRECATE

#include <boost/config.hpp>

#include <boost/core/lightweight_test.hpp>

// Boost.Bimap
#include <boost/bimap/support/lambda.hpp>
#include <boost/bimap/bimap.hpp>


void test_bimap_unconstrained()
{
    using namespace boost::bimaps;

    {
        typedef bimap<int,double,unconstrained_set_of_relation> bm;
        bm b;
        b.left.insert( bm::left_value_type(2,34.4) );
        b.right.insert( bm::right_value_type(2.2,3) );
    }

    {
        typedef bimap<int,unconstrained_set_of<double> > bm;
        bm b;
        b.insert( bm::value_type(2,34.4) );
        BOOST_TEST( b.size() == 1 );
    }

    {
        typedef bimap<unconstrained_set_of<int>, double > bm;
        bm b;
        b.right[2.4] = 34;
        BOOST_TEST( b.right.size() == 1 );
    }

    {
        typedef bimap<unconstrained_set_of<int>, double, right_based > bm;
        bm b;
        b.right[2.4] = 34;
        BOOST_TEST( b.right.size() == 1 );
    }

    {
        typedef bimap
        <
            int,
            unconstrained_set_of<double>,
            unconstrained_set_of_relation

        > bm;

        bm b;
        b.left[2] = 34.4;
        BOOST_TEST( b.left.size() == 1 );
    }

    {
        typedef bimap
        <
            unconstrained_set_of<int>,
            double,
            unconstrained_set_of_relation

        > bm;

        bm b;
        b.right[2.4] = 34;
        BOOST_TEST( b.right.size() == 1 );
    }

    {
        typedef bimap
        <
            unconstrained_set_of<int>,
            unconstrained_set_of<double>,
            set_of_relation<>

        > bm;

        bm b;
        b.insert( bm::value_type(1,2.3) );
        BOOST_TEST( b.size() == 1 );
    }
}


int main()
{
    test_bimap_unconstrained();
    return boost::report_errors();
}

