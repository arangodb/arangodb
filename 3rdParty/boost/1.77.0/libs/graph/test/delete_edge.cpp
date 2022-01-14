//=======================================================================
// Copyright 2018
// Authors: Rasmus Ahlberg
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#include <iostream>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_traits.hpp>

#include <boost/core/lightweight_test.hpp>

int main(int argc, char* argv[])
{
    typedef int Vertex;
    typedef int Edge;

    typedef boost::adjacency_list< boost::setS, // Container type for edges
        boost::vecS, // Container type for vertices
        boost::directedS, // Param for
        // directed/undirected/bidirectional
        // graph
        Vertex, // Type for the vertices
        Edge // Type for the edges
        >
        Graph_t;

    typedef Graph_t::edge_descriptor EdgeDesc;
    typedef Graph_t::vertex_descriptor VertexType;

    Graph_t m_graph;

    VertexType v1 = boost::add_vertex(m_graph);
    VertexType v2 = boost::add_vertex(m_graph);
    VertexType v3 = boost::add_vertex(m_graph);

    EdgeDesc ed1;
    bool inserted1;

    boost::tie(ed1, inserted1) = boost::add_edge(v3, v1, m_graph);

    BOOST_TEST(inserted1);

    static const int EDGE_VAL = 1234;

    m_graph[ed1] = EDGE_VAL;

    boost::remove_vertex(v2, m_graph);

    std::cout << "ed1 " << m_graph[ed1] << std::endl;

    BOOST_TEST(m_graph[ed1] == EDGE_VAL);

    return boost::report_errors();
}
