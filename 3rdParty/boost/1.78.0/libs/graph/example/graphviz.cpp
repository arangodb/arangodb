// Copyright 2005 Trustees of Indiana University

// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Author: Douglas Gregor
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>
#include <fstream>
#include <boost/graph/iteration_macros.hpp>

using namespace boost;

typedef boost::adjacency_list< vecS, vecS, directedS,
    property< vertex_name_t, std::string >, property< edge_weight_t, double > >
    Digraph;

typedef boost::adjacency_list< vecS, vecS, undirectedS,
    property< vertex_name_t, std::string >, property< edge_weight_t, double > >
    Graph;

void test_graph_read_write(const std::string& filename)
{
    std::ifstream in(filename.c_str());
    if (!BOOST_TEST(in)) {
        return;
    }

    Graph g;
    dynamic_properties dp;
    dp.property("id", get(vertex_name, g));
    dp.property("weight", get(edge_weight, g));
    BOOST_TEST(read_graphviz(in, g, dp, "id"));

    BOOST_TEST(num_vertices(g) == 4);
    BOOST_TEST(num_edges(g) == 4);

    typedef graph_traits< Graph >::vertex_descriptor Vertex;

    std::map< std::string, Vertex > name_to_vertex;
    BGL_FORALL_VERTICES(v, g, Graph)
    name_to_vertex[get(vertex_name, g, v)] = v;

    // Check vertices
    BOOST_TEST(name_to_vertex.find("0") != name_to_vertex.end());
    BOOST_TEST(name_to_vertex.find("1") != name_to_vertex.end());
    BOOST_TEST(name_to_vertex.find("foo") != name_to_vertex.end());
    BOOST_TEST(name_to_vertex.find("bar") != name_to_vertex.end());

    // Check edges
    BOOST_TEST(edge(name_to_vertex["0"], name_to_vertex["1"], g).second);
    BOOST_TEST(edge(name_to_vertex["1"], name_to_vertex["foo"], g).second);
    BOOST_TEST(edge(name_to_vertex["foo"], name_to_vertex["bar"], g).second);
    BOOST_TEST(edge(name_to_vertex["1"], name_to_vertex["bar"], g).second);

    BOOST_TEST(get(edge_weight, g,
                    edge(name_to_vertex["0"], name_to_vertex["1"], g).first)
        == 3.14159);
    BOOST_TEST(get(edge_weight, g,
                    edge(name_to_vertex["1"], name_to_vertex["foo"], g).first)
        == 2.71828);
    BOOST_TEST(get(edge_weight, g,
                    edge(name_to_vertex["foo"], name_to_vertex["bar"], g).first)
        == 10.0);
    BOOST_TEST(get(edge_weight, g,
                    edge(name_to_vertex["1"], name_to_vertex["bar"], g).first)
        == 10.0);

    // Write out the graph
    write_graphviz_dp(std::cout, g, dp, std::string("id"));
}

int main(int argc, char* argv[])
{
    test_graph_read_write(argc >= 2 ? argv[1] : "graphviz_example.dot");

    return boost::report_errors();
}
