
#include <boost/parameter.hpp>
#include <iostream>

namespace lib {

    BOOST_PARAMETER_NAME(name)
    BOOST_PARAMETER_NAME(index)

    BOOST_PARAMETER_FUNCTION(
        (int), f, tag, (optional (name,*,"bob")(index,(int),1))
    )
    {
        std::cout << name << ":" << index << std::endl;
        return index;
    }
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    int x = lib::f(lib::_name = "jill", lib::_index = 1);
    BOOST_TEST_EQ(x, 1);
    return boost::report_errors();
}

