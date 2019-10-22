
#include <boost/parameter.hpp>
#include <iostream>

namespace lib {
    namespace keywords {

        BOOST_PARAMETER_NAME(name)
        BOOST_PARAMETER_NAME(index)
    }

    BOOST_PARAMETER_FUNCTION(
        (int), f, keywords::tag, (optional (name,*,"bob")(index,(int),1))
    )
    {
        std::cout << name << ":" << index << std::endl;
        return index;
    }
}

#include <boost/core/lightweight_test.hpp>

using namespace lib::keywords;

int main()
{
    int x = lib::f(_name = "bob", _index = 2);
    BOOST_TEST_EQ(x, 2);
    return boost::report_errors();
}

