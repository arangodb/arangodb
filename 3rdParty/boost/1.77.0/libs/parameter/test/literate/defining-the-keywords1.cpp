
#include <boost/parameter/keyword.hpp>

namespace graphs {
    namespace tag {

        // keyword tag type
        struct graph
        {
            typedef boost::parameter::forward_reference qualifier;
        };
    }

    namespace // unnamed
    {
        // A reference to the keyword object
        boost::parameter::keyword<tag::graph> const& _graph
            = boost::parameter::keyword<tag::graph>::instance;
    }
}

