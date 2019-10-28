
#include <boost/parameter/name.hpp>

BOOST_PARAMETER_NAME(graph)
BOOST_PARAMETER_NAME(visitor)
BOOST_PARAMETER_NAME(root_vertex)
BOOST_PARAMETER_NAME(index_map)
BOOST_PARAMETER_NAME(in_out(color_map))

namespace boost {

    template <typename T = int>
    struct dfs_visitor
    {
    };

    int vertex_index = 0;
}

#include <boost/parameter/preprocessor.hpp>

namespace graphs {

    BOOST_PARAMETER_FUNCTION(
        (void),                // 1. parenthesized return type
        depth_first_search,    // 2. name of the function template

        tag,                   // 3. namespace of tag types

        (required (graph, *) ) // 4. one required parameter, and

        (optional              //    four optional parameters, with defaults
            (visitor,           *, boost::dfs_visitor<>())
            (root_vertex,       *, *vertices(graph).first)
            (index_map,         *, get(boost::vertex_index,graph))
            (in_out(color_map), *,
                default_color_map(num_vertices(graph), index_map)
            )
        )
    )
    {
        // ... body of function goes here...
        // use graph, visitor, index_map, and color_map
    }
}
