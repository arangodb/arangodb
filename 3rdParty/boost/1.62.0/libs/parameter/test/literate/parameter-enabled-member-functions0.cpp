
#include <boost/parameter.hpp>
#include <iostream>
using namespace boost::parameter;
BOOST_PARAMETER_NAME(arg1)
BOOST_PARAMETER_NAME(arg2)

struct callable2
{
    BOOST_PARAMETER_CONST_MEMBER_FUNCTION(
        (void), call, tag, (required (arg1,(int))(arg2,(int))))
    {
        std::cout << arg1 << ", " << arg2 << std::endl;
    }
};
