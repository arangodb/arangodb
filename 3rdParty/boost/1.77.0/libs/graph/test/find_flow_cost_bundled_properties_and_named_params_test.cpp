#include <boost/core/lightweight_test.hpp>
#include <boost/graph/successive_shortest_path_nonnegative_weights.hpp>
#include <boost/graph/find_flow_cost.hpp>
#include "min_cost_max_flow_utils.hpp"

typedef boost::adjacency_list_traits< boost::vecS, boost::vecS,
    boost::directedS >
    traits;
struct edge_t
{
    double capacity;
    float cost;
    float residual_capacity;
    traits::edge_descriptor reversed_edge;
};
struct node_t
{
    traits::edge_descriptor predecessor;
    int dist;
    int dist_prev;
    boost::vertex_index_t id;
};
typedef boost::adjacency_list< boost::listS, boost::vecS, boost::directedS,
    node_t, edge_t >
    Graph;

// Unit test written in order to fails (at compile time) if the find_flow_cost()
// is not properly handling bundled properties
void using_bundled_properties_with_find_max_flow_test()
{
    Graph g;
    traits::vertex_descriptor s, t;

    boost::property_map< Graph, double edge_t::* >::type capacity
        = get(&edge_t::capacity, g);
    boost::property_map< Graph, float edge_t::* >::type cost
        = get(&edge_t::cost, g);
    boost::property_map< Graph, float edge_t::* >::type residual_capacity
        = get(&edge_t::residual_capacity, g);
    boost::property_map< Graph, traits::edge_descriptor edge_t::* >::type rev
        = get(&edge_t::reversed_edge, g);
    boost::property_map< Graph, traits::edge_descriptor node_t::* >::type pred
        = get(&node_t::predecessor, g);
    boost::property_map< Graph, boost::vertex_index_t >::type vertex_indices
        = get(boost::vertex_index, g);
    boost::property_map< Graph, int node_t::* >::type dist
        = get(&node_t::dist, g);
    boost::property_map< Graph, int node_t::* >::type dist_prev
        = get(&node_t::dist_prev, g);

    boost::SampleGraph::getSampleGraph(
        g, s, t, capacity, residual_capacity, cost, rev);

    boost::successive_shortest_path_nonnegative_weights(g, s, t, capacity,
        residual_capacity, cost, rev, vertex_indices, pred, dist, dist_prev);

    // The "bundled properties" version (producing errors)
    int flow_cost = boost::find_flow_cost(g, capacity, residual_capacity, cost);
    BOOST_TEST_EQ(flow_cost, 29);
}

// Unit test written in order to fails (at compile time) if the find_flow_cost()
// is not properly handling bundled properties
void using_named_params_and_bundled_properties_with_find_max_flow_test()
{
    Graph g;
    traits::vertex_descriptor s, t;

    boost::property_map< Graph, double edge_t::* >::type capacity
        = get(&edge_t::capacity, g);
    boost::property_map< Graph, float edge_t::* >::type cost
        = get(&edge_t::cost, g);
    boost::property_map< Graph, float edge_t::* >::type residual_capacity
        = get(&edge_t::residual_capacity, g);
    boost::property_map< Graph, traits::edge_descriptor edge_t::* >::type rev
        = get(&edge_t::reversed_edge, g);
    boost::property_map< Graph, traits::edge_descriptor node_t::* >::type pred
        = get(&node_t::predecessor, g);
    boost::property_map< Graph, boost::vertex_index_t >::type vertex_indices
        = get(boost::vertex_index, g);
    boost::property_map< Graph, int node_t::* >::type dist
        = get(&node_t::dist, g);
    boost::property_map< Graph, int node_t::* >::type dist_prev
        = get(&node_t::dist_prev, g);

    boost::SampleGraph::getSampleGraph(
        g, s, t, capacity, residual_capacity, cost, rev);

    boost::successive_shortest_path_nonnegative_weights(g, s, t, capacity,
        residual_capacity, cost, rev, vertex_indices, pred, dist, dist_prev);

    // The  "named parameters" version (with "bundled properties"; producing
    // errors)
    int flow_cost = boost::find_flow_cost(g,
        boost::capacity_map(capacity)
            .residual_capacity_map(residual_capacity)
            .weight_map(cost));
    BOOST_TEST_EQ(flow_cost, 29);
}

int main()
{
    using_bundled_properties_with_find_max_flow_test();
    using_named_params_and_bundled_properties_with_find_max_flow_test();
    return boost::report_errors();
}
