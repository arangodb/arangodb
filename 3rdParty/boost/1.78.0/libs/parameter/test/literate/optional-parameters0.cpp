
#include <boost/parameter.hpp>

namespace boost {

    int vertex_index = 0;

    template <typename T = int>
    struct dfs_visitor
    {
    };
}

BOOST_PARAMETER_NAME(graph)
BOOST_PARAMETER_NAME(visitor)
BOOST_PARAMETER_NAME(root_vertex)
BOOST_PARAMETER_NAME(index_map)
BOOST_PARAMETER_NAME(in_out(color_map))

BOOST_PARAMETER_FUNCTION((void), f, tag,
    (required (graph, *))
    (optional
        (visitor,     *, boost::dfs_visitor<>())
        (root_vertex, *, *vertices(graph).first)
        (index_map,   *, get(boost::vertex_index,graph))
        (color_map,   *,
            default_color_map(num_vertices(graph), index_map)
        )
    )
)
{
}

