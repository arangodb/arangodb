// Copyright David Abrahams 2005.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/name.hpp>

namespace graphs {

    BOOST_PARAMETER_NAME(graph)    // Note: no semicolon
    BOOST_PARAMETER_NAME(visitor)
    BOOST_PARAMETER_NAME(root_vertex)
    BOOST_PARAMETER_NAME(index_map)
    BOOST_PARAMETER_NAME(color_map)
} // namespace graphs

#include <boost/core/lightweight_test.hpp>

namespace graphs { namespace core {

    template <typename ArgumentPack>
    void depth_first_search(ArgumentPack const& args)
    {
        BOOST_TEST_EQ(false, args[_color_map]);
        BOOST_TEST_EQ('G', args[_graph]);
        BOOST_TEST_CSTR_EQ("hello, world", args[_index_map]);
        BOOST_TEST_EQ(3.5, args[_root_vertex]);
        BOOST_TEST_EQ(2, args[_visitor]);
    }
}} // namespace graphs::core

int main()
{
    using namespace graphs;

    core::depth_first_search((
        _graph = 'G', _visitor = 2, _root_vertex = 3.5
      , _index_map = "hello, world", _color_map = false
    ));
    return boost::report_errors();
}
