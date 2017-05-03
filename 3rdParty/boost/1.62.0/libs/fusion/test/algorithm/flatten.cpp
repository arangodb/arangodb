/*==============================================================================
    Copyright (c) 2013 Jamboree

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/sequence/io/out.hpp>
#include <boost/fusion/sequence/comparison/equal_to.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/sequence/intrinsic/begin.hpp>
#include <boost/fusion/sequence/intrinsic/end.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/sequence/intrinsic/size.hpp>
#include <boost/fusion/iterator/advance.hpp>
#include <boost/fusion/iterator/deref.hpp>
#include <boost/fusion/iterator/distance.hpp>
#include <boost/fusion/algorithm/auxiliary/copy.hpp>
#include <boost/fusion/algorithm/transformation/flatten.hpp>


int main()
{
    using namespace boost::fusion;

    {
        typedef vector<int, int, vector<int, int>, int> sequence_type;
        sequence_type seq(1, 2, make_vector(3, 4), 5);

        BOOST_TEST((boost::fusion::size(flatten(seq)) == 5));
    }
        
    {
        typedef vector<int, int, vector<int, int>, int> sequence_type;
        sequence_type seq(1, 2, make_vector(3, 4), 5);
        std::cout << flatten(seq) << std::endl;
        BOOST_TEST((flatten(seq) == make_vector(1, 2, 3, 4, 5)));
    }

    {
        std::cout << flatten(make_vector(1, 2, make_vector(3, 4), 5)) << std::endl;
        BOOST_TEST((flatten(make_vector(1, 2, make_vector(3, 4), 5)) == make_vector(1, 2, 3, 4, 5)));
    }
    
    {
        typedef vector<int, int, vector<int, int>, int> sequence_type;
        sequence_type seq;
        result_of::flatten<sequence_type>::type flat(flatten(seq));
        copy(make_vector(1, 2, 3, 4, 5), flat);
        std::cout << seq << std::endl;
        BOOST_TEST((seq == make_vector(1, 2, make_vector(3, 4), 5)));
    }
    
    return boost::report_errors();
}

