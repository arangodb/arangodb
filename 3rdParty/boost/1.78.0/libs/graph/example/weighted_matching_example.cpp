//=======================================================================
// Copyright (c) 2018 Yi Ji
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//=======================================================================

#include <iostream>
#include <vector>
#include <string>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/maximum_weighted_matching.hpp>

using namespace boost;

typedef property< edge_weight_t, float, property< edge_index_t, int > >
    EdgeProperty;
typedef adjacency_list< vecS, vecS, undirectedS, no_property, EdgeProperty >
    my_graph;

int main(int argc, const char* argv[])
{
    graph_traits< my_graph >::vertex_iterator vi, vi_end;
    const int n_vertices = 18;
    my_graph g(n_vertices);

    // vertices can be refered by integers because my_graph use vector to store
    // them

    add_edge(1, 2, EdgeProperty(5), g);
    add_edge(0, 4, EdgeProperty(1), g);
    add_edge(1, 5, EdgeProperty(4), g);
    add_edge(2, 6, EdgeProperty(1), g);
    add_edge(3, 7, EdgeProperty(4), g);
    add_edge(4, 5, EdgeProperty(7), g);
    add_edge(6, 7, EdgeProperty(5), g);
    add_edge(4, 8, EdgeProperty(2), g);
    add_edge(5, 9, EdgeProperty(5), g);
    add_edge(6, 10, EdgeProperty(6), g);
    add_edge(7, 11, EdgeProperty(5), g);
    add_edge(10, 11, EdgeProperty(4), g);
    add_edge(8, 13, EdgeProperty(4), g);
    add_edge(9, 14, EdgeProperty(4), g);
    add_edge(10, 15, EdgeProperty(7), g);
    add_edge(11, 16, EdgeProperty(6), g);
    add_edge(14, 15, EdgeProperty(6), g);
    add_edge(12, 13, EdgeProperty(2), g);
    add_edge(16, 17, EdgeProperty(5), g);

    // print the ascii graph into terminal (better to use fixed-width font)
    // this graph has a maximum cardinality matching of size 8
    // but maximum weighted matching is of size 7

    std::vector< std::string > ascii_graph_weighted;

    ascii_graph_weighted.push_back("                     5                 ");
    ascii_graph_weighted.push_back("           A       B---C       D       ");
    ascii_graph_weighted.push_back("           1\\  7  /4   1\\  5  /4     ");
    ascii_graph_weighted.push_back("             E---F       G---H         ");
    ascii_graph_weighted.push_back("            2|   |5     6|   |5        ");
    ascii_graph_weighted.push_back("             I   J       K---L         ");
    ascii_graph_weighted.push_back("           4/    3\\    7/  4  \\6     ");
    ascii_graph_weighted.push_back("       M---N       O---P       Q---R   ");
    ascii_graph_weighted.push_back("         2           6           5     ");

    // our maximum weighted matching and result

    std::cout << "In the following graph:" << std::endl << std::endl;

    for (std::vector< std::string >::iterator itr
         = ascii_graph_weighted.begin();
         itr != ascii_graph_weighted.end(); ++itr)
        std::cout << *itr << std::endl;

    std::cout << std::endl;

    std::vector< graph_traits< my_graph >::vertex_descriptor > mate1(
        n_vertices),
        mate2(n_vertices);
    maximum_weighted_matching(g, &mate1[0]);

    std::cout << "Found a weighted matching:" << std::endl;
    std::cout << "Matching size is " << matching_size(g, &mate1[0])
              << ", total weight is " << matching_weight_sum(g, &mate1[0])
              << std::endl;
    std::cout << std::endl;

    std::cout << "The matching is:" << std::endl;
    for (boost::tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi)
        if (mate1[*vi] != graph_traits< my_graph >::null_vertex()
            && *vi < mate1[*vi])
            std::cout << "{" << *vi << ", " << mate1[*vi] << "}" << std::endl;
    std::cout << std::endl;

    // now we check the correctness by compare the weight sum to a brute-force
    // matching result note that two matchings may be different because of
    // multiple optimal solutions

    brute_force_maximum_weighted_matching(g, &mate2[0]);

    std::cout << "Found a weighted matching by brute-force searching:"
              << std::endl;
    std::cout << "Matching size is " << matching_size(g, &mate2[0])
              << ", total weight is " << matching_weight_sum(g, &mate2[0])
              << std::endl;
    std::cout << std::endl;

    std::cout << "The brute-force matching is:" << std::endl;
    for (boost::tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi)
        if (mate2[*vi] != graph_traits< my_graph >::null_vertex()
            && *vi < mate2[*vi])
            std::cout << "{" << *vi << ", " << mate2[*vi] << "}" << std::endl;
    std::cout << std::endl;

    assert(
        matching_weight_sum(g, &mate1[0]) == matching_weight_sum(g, &mate2[0]));

    return 0;
}
