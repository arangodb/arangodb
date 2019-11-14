
#include <boost/parameter.hpp>

BOOST_PARAMETER_NAME((pass_foo, keywords) foo)

BOOST_PARAMETER_FUNCTION(
    (int), f, keywords, (required (foo, *))
)
{
    return foo + 1;
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    int x = f(pass_foo = 41);
    BOOST_TEST_EQ(x, 42);
    return boost::report_errors();
}

