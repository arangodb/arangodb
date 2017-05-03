
#include <boost/parameter.hpp>
#include <iostream>

BOOST_PARAMETER_NAME(graph)
BOOST_PARAMETER_NAME(visitor)
BOOST_PARAMETER_NAME(root_vertex)
BOOST_PARAMETER_NAME(index_map)
BOOST_PARAMETER_NAME(color_map)
#include <boost/graph/depth_first_search.hpp> // for dfs_visitor

BOOST_PARAMETER_FUNCTION(
    (void), depth_first_search, tag
    
, (required
    (graph, *)
    (visitor, *)
    (root_vertex, *)
    (index_map, *)
    (color_map, *)
  )

)
{
   std::cout << "graph=" << graph << std::endl;
   std::cout << "visitor=" << visitor << std::endl;
   std::cout << "root_vertex=" << root_vertex << std::endl;
   std::cout << "index_map=" << index_map << std::endl;
   std::cout << "color_map=" << color_map << std::endl;
}

int main()
{
    depth_first_search(1, 2, 3, 4, 5);

    depth_first_search(
        "1", '2', _color_map = '5',
        _index_map = "4", _root_vertex = "3");
}
