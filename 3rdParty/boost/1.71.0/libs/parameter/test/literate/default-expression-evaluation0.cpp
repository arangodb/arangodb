
#include <boost/parameter.hpp>
#include <iostream>

BOOST_PARAMETER_NAME(graph)
BOOST_PARAMETER_NAME(visitor)
BOOST_PARAMETER_NAME(root_vertex)
BOOST_PARAMETER_NAME(index_map)
BOOST_PARAMETER_NAME(color_map)

#include <boost/graph/depth_first_search.hpp> // for dfs_visitor

BOOST_PARAMETER_FUNCTION((bool), depth_first_search, tag,
    (required
        (graph, *)
        (visitor, *)
        (root_vertex, *)
        (index_map, *)
        (color_map, *)
    )
)
{
    std::cout << "graph=" << graph;
    std::cout << std::endl;
    std::cout << "visitor=" << visitor;
    std::cout << std::endl;
    std::cout << "root_vertex=" << root_vertex;
    std::cout << std::endl;
    std::cout << "index_map=" << index_map;
    std::cout << std::endl;
    std::cout << "color_map=" << color_map;
    std::cout << std::endl;
    return true;
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    char const* g = "1";
    depth_first_search(1, 2, 3, 4, 5);
    depth_first_search(
        g, '2', _color_map = '5'
      , _index_map = "4", _root_vertex = "3"
    );
    return boost::report_errors();
}

