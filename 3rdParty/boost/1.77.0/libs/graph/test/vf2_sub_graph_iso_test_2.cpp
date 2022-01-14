//=======================================================================
// Boost.Graph library vf2_sub_graph_iso test 2
// Test of return value and behaviour with empty graphs
//
// Copyright (C) 2013 Jakob Lykke Andersen, University of Southern Denmark
// (jlandersen@imada.sdu.dk)
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//=======================================================================

#include <iostream>
#include <boost/core/lightweight_test.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/vf2_sub_graph_iso.hpp>

struct test_callback
{
    test_callback(bool& got_hit, bool stop) : got_hit(got_hit), stop(stop) {}

    template < typename Map1To2, typename Map2To1 >
    bool operator()(Map1To2 map1to2, Map2To1 map2to1)
    {
        got_hit = true;
        return stop;
    }

    bool& got_hit;
    bool stop;
};

struct false_predicate
{
    template < typename VertexOrEdge1, typename VertexOrEdge2 >
    bool operator()(VertexOrEdge1 ve1, VertexOrEdge2 ve2) const
    {
        return false;
    }
};

void test_empty_graph_cases()
{
    typedef boost::adjacency_list< boost::vecS, boost::vecS,
        boost::bidirectionalS >
        Graph;
    Graph gEmpty, gLarge;
    add_vertex(gLarge);

    { // isomorphism
        bool got_hit = false;
        test_callback callback(got_hit, true);
        bool exists = vf2_graph_iso(gEmpty, gEmpty, callback);
        BOOST_TEST(exists);
        BOOST_TEST(got_hit); // even empty matches are reported
    }
    { // subgraph isomorphism (induced)
        { // empty vs. empty
            bool got_hit = false;
    test_callback callback(got_hit, true);
    bool exists = vf2_subgraph_iso(gEmpty, gEmpty, callback);
    BOOST_TEST(exists);
    BOOST_TEST(got_hit); // even empty matches are reported
}
{ // empty vs. non-empty
    bool got_hit = false;
    test_callback callback(got_hit, true);
    bool exists = vf2_subgraph_iso(gEmpty, gLarge, callback);
    BOOST_TEST(exists);
    BOOST_TEST(got_hit); // even empty matches are reported
}
}
{ // subgraph monomorphism (non-induced subgraph isomorphism)
    { // empty vs. empty
        bool got_hit = false;
        test_callback callback(got_hit, true);
        bool exists = vf2_subgraph_mono(gEmpty, gEmpty, callback);
        BOOST_TEST(exists);
        BOOST_TEST(got_hit); // even empty matches are reported
    }
    { // empty vs. non-empty
        bool got_hit = false;
        test_callback callback(got_hit, true);
        bool exists = vf2_subgraph_mono(gEmpty, gLarge, callback);
        BOOST_TEST(exists);
        BOOST_TEST(got_hit); // even empty matches are reported
    }
}
}

void test_return_value()
{
    typedef boost::adjacency_list< boost::vecS, boost::vecS,
        boost::bidirectionalS >
        Graph;
    typedef boost::graph_traits< Graph >::vertex_descriptor Vertex;
    Graph gSmall, gLarge;
    add_vertex(gSmall);
    Vertex v1 = add_vertex(gLarge);
    Vertex v2 = add_vertex(gLarge);
    add_edge(v1, v2, gLarge);

    { // isomorphism
        { // no morphism due to sizes
            bool got_hit = false;
    test_callback callback(got_hit, true);
    bool exists = vf2_graph_iso(gSmall, gLarge, callback);
    BOOST_TEST(!exists);
    BOOST_TEST(!got_hit);
}
{ // no morphism due to vertex mismatches
    bool got_hit = false;
    test_callback callback(got_hit, true);
    false_predicate pred;
    bool exists
        = vf2_graph_iso(gLarge, gLarge, callback, vertex_order_by_mult(gLarge),
            boost::edges_equivalent(pred).vertices_equivalent(pred));
    BOOST_TEST(!exists);
    BOOST_TEST(!got_hit);
}
{ // morphism, find all
    bool got_hit = false;
    test_callback callback(got_hit, false);
    bool exists = vf2_graph_iso(gLarge, gLarge, callback);
    BOOST_TEST(exists);
    BOOST_TEST(got_hit);
}
{ // morphism, stop after first hit
    bool got_hit = false;
    test_callback callback(got_hit, true);
    bool exists = vf2_graph_iso(gLarge, gLarge, callback);
    BOOST_TEST(exists);
    BOOST_TEST(got_hit);
}
}
{ // subgraph isomorphism (induced)
    { // no morphism due to sizes
        bool got_hit = false;
test_callback callback(got_hit, true);
bool exists = vf2_subgraph_iso(gLarge, gSmall, callback);
BOOST_TEST(!exists);
BOOST_TEST(!got_hit);
}
{ // no morphism due to vertex mismatches
    bool got_hit = false;
    test_callback callback(got_hit, true);
    false_predicate pred;
    bool exists = vf2_subgraph_iso(gLarge, gLarge, callback,
        vertex_order_by_mult(gLarge),
        boost::edges_equivalent(pred).vertices_equivalent(pred));
    BOOST_TEST(!exists);
    BOOST_TEST(!got_hit);
}
{ // morphism, find all
    bool got_hit = false;
    test_callback callback(got_hit, false);
    bool exists = vf2_subgraph_iso(gLarge, gLarge, callback);
    BOOST_TEST(exists);
    BOOST_TEST(got_hit);
}
{ // morphism, stop after first hit
    bool got_hit = false;
    test_callback callback(got_hit, true);
    bool exists = vf2_subgraph_iso(gLarge, gLarge, callback);
    BOOST_TEST(exists);
    BOOST_TEST(got_hit);
}
}
{ // subgraph monomorphism (non-induced subgraph isomorphism)
    { // no morphism due to sizes
        bool got_hit = false;
        test_callback callback(got_hit, true);
        bool exists = vf2_subgraph_mono(gLarge, gSmall, callback);
        BOOST_TEST(!exists);
        BOOST_TEST(!got_hit);
    }
    { // no morphism due to vertex mismatches
        bool got_hit = false;
        test_callback callback(got_hit, true);
        false_predicate pred;
        bool exists = vf2_subgraph_mono(gLarge, gLarge, callback,
            vertex_order_by_mult(gLarge),
            boost::edges_equivalent(pred).vertices_equivalent(pred));
        BOOST_TEST(!exists);
        BOOST_TEST(!got_hit);
    }
    { // morphism, find all
        bool got_hit = false;
        test_callback callback(got_hit, false);
        bool exists = vf2_subgraph_mono(gLarge, gLarge, callback);
        BOOST_TEST(exists);
        BOOST_TEST(got_hit);
    }
    { // morphism, stop after first hit
        bool got_hit = false;
        test_callback callback(got_hit, true);
        bool exists = vf2_subgraph_mono(gLarge, gLarge, callback);
        BOOST_TEST(exists);
        BOOST_TEST(got_hit);
    }
}
}

int main(int argc, char* argv[])
{
    test_empty_graph_cases();
    test_return_value();
    return boost::report_errors();
}
