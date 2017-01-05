// Copyright (C) Ben Pope 2014.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/graph/directed_graph.hpp>
#include <boost/graph/undirected_graph.hpp>

template<typename Graph>
void test_member_swap()
{
  Graph lhs, rhs;
  lhs.swap(rhs);
}

int main()
{
    test_member_swap<boost::adjacency_list<> >();
    test_member_swap<boost::directed_graph<> >();
    test_member_swap<boost::undirected_graph<> >();
}
