
#include <boost/parameter.hpp>

BOOST_PARAMETER_NAME(graph)

BOOST_PARAMETER_FUNCTION((void), f, tag,
(required (graph, *) )
) {}

