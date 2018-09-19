#define BOOST_TEST_MODULE max_flow_algorithms_named_parameters_and_bundled_params_test

#include <boost/graph/adjacency_list.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/graph/edmonds_karp_max_flow.hpp>

#include "min_cost_max_flow_utils.hpp"

typedef boost::adjacency_list_traits<boost::vecS,boost::vecS,boost::directedS> traits;
struct edge_t {
  double capacity;
  float cost;
  float residual_capacity;
  traits::edge_descriptor reversed_edge;
};
struct node_t {
  traits::edge_descriptor predecessor;
  int dist;
  int dist_prev;
  boost::vertex_index_t id;
  boost::default_color_type color;
};
typedef boost::adjacency_list<boost::listS, boost::vecS, boost::directedS, node_t, edge_t > Graph;

BOOST_AUTO_TEST_CASE(using_named_parameters_and_bundled_params_on_edmonds_karp_max_flow_test)
{
  Graph g;
  traits::vertex_descriptor s,t;

  boost::property_map<Graph,double edge_t::* >::type capacity = get(&edge_t::capacity, g);
  boost::property_map<Graph,float edge_t::* >::type cost = get(&edge_t::cost, g);
  boost::property_map<Graph,float edge_t::* >::type residual_capacity = get(&edge_t::residual_capacity, g);
  boost::property_map<Graph,traits::edge_descriptor edge_t::* >::type rev = get(&edge_t::reversed_edge, g);
  boost::property_map<Graph,traits::edge_descriptor node_t::* >::type pred = get(&node_t::predecessor, g);
  boost::property_map<Graph,boost::default_color_type node_t::* >::type col = get(&node_t::color, g);

  boost::SampleGraph::getSampleGraph(g,s,t,capacity,residual_capacity,cost,rev);

  // The "named parameter version" (producing errors)
  // I chose to show the error with edmonds_karp_max_flow().
  int flow_value = edmonds_karp_max_flow(g, s, t,
    boost::capacity_map(capacity)
    .residual_capacity_map(residual_capacity)
    .reverse_edge_map(rev)
    .color_map(col)
    .predecessor_map(pred));

  BOOST_CHECK_EQUAL(flow_value,4);
}
