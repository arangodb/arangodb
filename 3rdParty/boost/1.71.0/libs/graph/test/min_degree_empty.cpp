//=======================================================================
// Copyright 2017 Felix Salfelder
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#include <boost/graph/minimum_degree_ordering.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/test/minimal.hpp>
#include <boost/typeof/typeof.hpp>
#include <vector>
#include <map>

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> G;

int test_main(int argc, char** argv)
{
    size_t n = 10;
    G g(n);

    std::vector<int> inverse_perm(n, 0);
    std::vector<int> supernode_sizes(n, 1);
    BOOST_AUTO(id, boost::get(boost::vertex_index, g));
    std::vector<int> degree(n, 0);
    std::map<int,int> io;
    std::map<int,int> o;

    boost::minimum_degree_ordering(
        g
      , boost::make_iterator_property_map(degree.begin(), id, degree[0])
      , boost::make_assoc_property_map(io)
      , boost::make_assoc_property_map(o)
      , boost::make_iterator_property_map(
            supernode_sizes.begin()
          , id
          , supernode_sizes[0]
        )
      , 0
      , id
    );

    for (int k = 0; k < n; ++k)
    {
        BOOST_CHECK(o[io[k]] == k);
    }

    return 0;
}
