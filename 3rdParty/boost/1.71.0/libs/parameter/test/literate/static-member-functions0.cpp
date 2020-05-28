
#include <boost/parameter.hpp>
#include <iostream>

using namespace boost::parameter;

BOOST_PARAMETER_NAME(arg1)

struct somebody
{
    BOOST_PARAMETER_MEMBER_FUNCTION(
        (void), static f, tag, (optional (arg1,(int),0))
    )
    {
        std::cout << arg1 << std::endl;
    }
};

#include <boost/core/lightweight_test.hpp>

int main()
{
    somebody::f();
    somebody::f(4);
    return boost::report_errors();
}

