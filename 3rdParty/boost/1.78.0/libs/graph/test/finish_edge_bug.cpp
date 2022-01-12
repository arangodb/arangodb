// Author: Alex Lauser

// Output (Note that 'finish_edge' is never printed):
// The example graph:
// 0 --> 1 2
// 1 --> 2
// 2 --> 0

#include <boost/config.hpp>
#include <iostream>
#include <vector>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>

template < typename graph_t > struct TalkativeVisitor : boost::dfs_visitor<>
{
    typedef typename boost::graph_traits< graph_t >::vertex_descriptor
        vertex_descriptor;
    typedef typename boost::graph_traits< graph_t >::edge_descriptor
        edge_descriptor;

    // // Commented out to avoid clutter of the output.
    // void discover_vertex(vertex_descriptor u, const graph_t&) { // check!
    //   std::cout << "discover_vertex: " << u << std::endl;
    // }
    // void finish_vertex(vertex_descriptor u, const graph_t&) { // check!
    //     std::cout << "finish_vertex: " << u << std::endl;
    // }
    // void initialize_vertex(vertex_descriptor u, const graph_t&) { // check!
    //     std::cout << "initialize_vertex: " << u << std::endl;
    // }
    // void start_vertex(vertex_descriptor u, const graph_t&) { // check!
    //     std::cout << "start_vertex: " << u << std::endl;
    // }
    // void examine_edge(edge_descriptor u, const graph_t&) { // check!
    //     std::cout << "examine_edge: " << u << std::endl;
    // }
    // void tree_edge(edge_descriptor u, const graph_t&) { // check!
    //     std::cout << "tree_edge: " << u << std::endl;
    // }
    // void back_edge(edge_descriptor u, const graph_t&) { // check!
    //     std::cout << "back_edge: " << u << std::endl;
    // }
    // void forward_or_cross_edge(edge_descriptor u, const graph_t&) { // check!
    //     std::cout << "forward_or_cross_edge: " << u << std::endl;
    // }
    void finish_edge(edge_descriptor u, const graph_t&)
    { // uncalled!
        std::cout << "finish_edge: " << u << std::endl;
    }
};

template < typename t >
std::ostream& operator<<(std::ostream& os, const std::pair< t, t >& x)
{
    return os << "(" << x.first << ", " << x.second << ")";
}

int main(int, char*[])
{
    using namespace boost;

    typedef adjacency_list< vecS, vecS, directedS > Graph;
    Graph G;

    typedef graph_traits<
        adjacency_list< vecS, vecS, directedS > >::vertex_descriptor Vertex;
    Vertex a = add_vertex(G);
    Vertex b = add_vertex(G);
    Vertex c = add_vertex(G);

    add_edge(a, b, G);
    add_edge(b, c, G);
    add_edge(c, a, G);
    add_edge(a, c, G);

    std::cout << "The example graph:" << std::endl;
    print_graph(G);

    std::vector< default_color_type > color(num_vertices(G));
    depth_first_search(G, visitor(TalkativeVisitor< Graph >()));

    return 0;
}
