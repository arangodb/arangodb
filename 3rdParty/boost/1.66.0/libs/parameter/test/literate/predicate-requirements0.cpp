
#include <boost/parameter.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>

BOOST_PARAMETER_NAME((_graph, graphs) graph)
BOOST_PARAMETER_NAME((_visitor, graphs) visitor)
BOOST_PARAMETER_NAME((_root_vertex, graphs) root_vertex)
BOOST_PARAMETER_NAME((_index_map, graphs) index_map)
BOOST_PARAMETER_NAME((_color_map, graphs) color_map)

using boost::mpl::_;
// We first need to define a few metafunction that we use in the
// predicates below.

template <class G>
struct traversal_category
{
    typedef typename boost::graph_traits<G>::traversal_category type;
};

template <class G>
struct vertex_descriptor
{
    typedef typename boost::graph_traits<G>::vertex_descriptor type;
};

template <class G>
struct value_type
{
    typedef typename boost::property_traits<G>::value_type type;
};

template <class G>
struct key_type
{
    typedef typename boost::property_traits<G>::key_type type;
};

template<class Size, class IndexMap>
boost::iterator_property_map<
    boost::default_color_type*, IndexMap
  , boost::default_color_type, boost::default_color_type&
>
default_color_map(Size num_vertices, IndexMap const& index_map)
{
   std::vector<boost::default_color_type> colors(num_vertices);
   return &colors[0];
}

BOOST_PARAMETER_FUNCTION(
    (void), depth_first_search, graphs

  , (required
      (graph
       , *(boost::mpl::and_<
               boost::is_convertible<
                   traversal_category<_>, boost::incidence_graph_tag
               >
             , boost::is_convertible<
                   traversal_category<_>, boost::vertex_list_graph_tag
               >
           >) ))

    (optional
      (visitor, *, boost::dfs_visitor<>()) // not checkable

      (root_vertex
        , (vertex_descriptor<graphs::graph::_>)
        , *vertices(graph).first)

      (index_map
        , *(boost::mpl::and_<
              boost::is_integral<value_type<_> >
            , boost::is_same<
                  vertex_descriptor<graphs::graph::_>, key_type<_>
              >
          >)
        , get(boost::vertex_index,graph))

      (in_out(color_map)
        , *(boost::is_same<
              vertex_descriptor<graphs::graph::_>, key_type<_>
          >)
       , default_color_map(num_vertices(graph), index_map) )
    )
)
{}

int main()
{
    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> G;

    enum {u, v, w, x, y, z, N};
    typedef std::pair<int, int> E;
    E edges[] = {E(u, v), E(u, x), E(x, v), E(y, x), E(v, y), E(w, y),
                 E(w,z), E(z, z)};
    G g(edges, edges + sizeof(edges) / sizeof(E), N);

    ::depth_first_search(g);
    ::depth_first_search(g, _root_vertex = (int)x);
}

