
#include <boost/parameter.hpp>

BOOST_PARAMETER_NAME(index)

template <typename ArgumentPack>
typename boost::parameter::value_type<ArgumentPack,tag::index,int>::type
    twice_index(ArgumentPack const& args)
{
    return 2 * args[_index|42];
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    int six = twice_index(_index = 3);
    BOOST_TEST_EQ(six, 6);
    return boost::report_errors();
}

