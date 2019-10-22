//=======================================================================
// Copyright (c) 2018 Yi Ji
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//=======================================================================

#include <boost/graph/max_cardinality_matching.hpp>
#include <boost/graph/maximum_weighted_matching.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/adjacency_matrix.hpp>
#include <boost/test/minimal.hpp>

using namespace boost;

typedef property<edge_weight_t, float, property<edge_index_t, int> > EdgeProperty;

typedef adjacency_list<vecS, vecS, undirectedS, property<vertex_index_t,int>, EdgeProperty> undirected_graph;
typedef adjacency_list<listS, listS, undirectedS, property<vertex_index_t,int>, EdgeProperty> undirected_list_graph;
typedef adjacency_matrix<undirectedS, property<vertex_index_t,int>, EdgeProperty> undirected_adjacency_matrix_graph;

template <typename Graph>
struct vertex_index_installer
{
    static void install(Graph&) {}
};

template <>
struct vertex_index_installer<undirected_list_graph>
{
    static void install(undirected_list_graph& g)
    {
        typedef graph_traits<undirected_list_graph>::vertex_iterator vertex_iterator_t;
        typedef graph_traits<undirected_list_graph>::vertices_size_type v_size_t;
        
        vertex_iterator_t vi, vi_end;
        v_size_t i = 0;
        for (boost::tie(vi,vi_end) = vertices(g); vi != vi_end; ++vi, ++i)
            put(vertex_index, g, *vi, i);
    }
};

template <typename Graph>
void print_graph(const Graph& g)
{
    typedef typename graph_traits<Graph>::edge_iterator edge_iterator_t;
    edge_iterator_t ei, ei_end;
    std::cout << std::endl << "The graph is: " << std::endl;
    for (boost::tie(ei,ei_end) = edges(g); ei != ei_end; ++ei)
        std::cout << "add_edge(" << source(*ei, g) << ", " << target(*ei, g) << ", EdgeProperty(" << get(edge_weight, g, *ei) << "), );" << std::endl;
}

template <typename Graph>
void weighted_matching_test(const Graph& g, 
                            typename property_traits<typename property_map<Graph, edge_weight_t>::type>::value_type answer)
{
    typedef typename property_map<Graph,vertex_index_t>::type vertex_index_map_t;
    typedef vector_property_map< typename graph_traits<Graph>::vertex_descriptor, vertex_index_map_t > mate_t;
    mate_t mate(num_vertices(g));
    maximum_weighted_matching(g, mate);
    bool same_result = (matching_weight_sum(g, mate) == answer);
    BOOST_CHECK(same_result);
    if (!same_result)
    {
        mate_t max_mate(num_vertices(g));
        brute_force_maximum_weighted_matching(g, max_mate);

        std::cout << std::endl << "Found a weighted matching of weight sum " << matching_weight_sum(g, mate) << std::endl
        << "While brute-force search found a weighted matching of weight sum " << matching_weight_sum(g, max_mate) << std::endl;
        
        typedef typename graph_traits<Graph>::vertex_iterator vertex_iterator_t;
        vertex_iterator_t vi, vi_end;
        
        print_graph(g);
        
        std::cout << std::endl << "The algorithmic matching is:" << std::endl;
        for (boost::tie(vi,vi_end) = vertices(g); vi != vi_end; ++vi)
            if (mate[*vi] != graph_traits<Graph>::null_vertex() && *vi < mate[*vi])
                std::cout << "{" << *vi << ", " << mate[*vi] << "}" << std::endl;
        
        std::cout << std::endl << "The brute-force matching is:" << std::endl;
        for (boost::tie(vi,vi_end) = vertices(g); vi != vi_end; ++vi)
            if (max_mate[*vi] != graph_traits<Graph>::null_vertex() && *vi < max_mate[*vi])
                std::cout << "{" << *vi << ", " << max_mate[*vi] << "}" << std::endl;
        
        std::cout << std::endl;
    }
}

template <typename Graph>
Graph make_graph(typename graph_traits<Graph>::vertices_size_type num_v, 
                 typename graph_traits<Graph>::edges_size_type num_e,
                 std::deque<std::size_t> input_edges)
{
    Graph g(num_v);
    vertex_index_installer<Graph>::install(g);
    for (std::size_t i = 0; i < num_e; ++i)
    {
        std::size_t src_v, tgt_v, edge_weight;
        src_v = input_edges.front();
        input_edges.pop_front();
        tgt_v = input_edges.front();
        input_edges.pop_front();
        edge_weight = input_edges.front();
        input_edges.pop_front();
        add_edge(vertex(src_v, g), vertex(tgt_v, g), EdgeProperty(edge_weight), g);
    }
    return g;
}

int test_main(int, char*[])
{
    std::ifstream in_file("weighted_matching.dat");
    std::string line;
    while (std::getline(in_file, line))
    {
        std::istringstream in_graph(line);
        std::size_t answer, num_v, num_e;
        in_graph >> answer >> num_v >> num_e;

        std::deque<std::size_t> input_edges;
        std::size_t i;
        while (in_graph >> i)
            input_edges.push_back(i);
        
        weighted_matching_test(make_graph<undirected_graph>(num_v, num_e, input_edges), answer);
        weighted_matching_test(make_graph<undirected_list_graph>(num_v, num_e, input_edges), answer);
        weighted_matching_test(make_graph<undirected_adjacency_matrix_graph>(num_v, num_e, input_edges), answer);
    }
    return 0;
}


